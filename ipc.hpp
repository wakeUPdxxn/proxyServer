#pragma once
#include <boost/interprocess/ipc/message_queue.hpp>
#include <queue>
#include <string>

namespace InterProcess {
	namespace bipc = boost::interprocess;

	struct Data {             //Data storage for sending to another proccess
		Data() = default;
		Data(const Data& other) = delete;              //move only object
		Data& operator=(const Data& other) = delete;   //move only object

		std::string _targetId; //required field

		struct targetInfo { //all fields are required
			std::string os;
			std::string resolution;
			std::string hostName;
		}_targetInfo;

		struct BrowserData { //all fields are required
			std::string browserName;
			std::vector<std::tuple<std::string, std::string, std::string>>resources;
		}_browserData;
	};

	class IPCworkDispatcher { //class that encapsulates and implements parallel message queue processing
	protected:
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
		void putInQueue(std::unique_ptr<Data>&&data) {     //this func will be called from multiple parser threads through the IPC.newData()
			std::lock_guard<std::mutex>dataLock(dataMt);  //2 PARSER THREADS PUTTS DATA IN QUEUE AND THEN 1 CURRENT WORKER GETTS IT
			dataQueue.push(std::move(data));
		}
	private:
		void dataWaiter() {     //{spinlocked} waiting for new data and then then calls the specific callback
			while (!stop_flag) {                
				if (!dataQueue.empty()) {
					callback(dataQueue.front());

					std::lock_guard<std::mutex>dataLock(dataMt);
					dataQueue.pop();
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
		std::queue<std::unique_ptr<Data>>dataQueue; //now its onws all data ptrs created in server parser func

		std::function<void(const std::unique_ptr<Data>&)>callback;
	};

	class IPC {

	public:
		IPC() {
			wd = IPCworkDispatcher::getInstance();
			//bipc::message_queue::remove("msg_queue");
			//msg_queue = std::make_unique<bipc::message_queue>(bipc::create_only, "msg_queue", 100, sizeof(Data));
		}
		~IPC() = default;

		void _init() { //make able to perform ipc operations;
			wd->setCallback(std::bind(&IPC::sendData, this, std::placeholders::_1));
			wd->start(); //execute worker thread
		}
		void newData(std::unique_ptr<Data>&&data) const { //no effect before _init() call
			wd->putInQueue(std::move(data));
		}
	private:
		//std::unique_ptr<bipc::message_queue>msg_queue;
		std::shared_ptr<IPCworkDispatcher>wd;
	private:
		void sendData(const std::unique_ptr<Data>&data) { //This callback be executed in another thread from IPCworkDispatcher
			                                              //const data coz ptr's lifetime ends at dataQueue.pop() in dataWaiter.
				                                         //UB if data will be erased early;
		}
	};
};