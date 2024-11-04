#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/move/move.hpp>
#include <boost/json.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <queue>
#include <condition_variable>

namespace ServerSide {
	namespace ba = boost::asio;
	using ba::ip::tcp;
	using err_c = boost::system::error_code;

	using RequestHandler = std::function<void(const std::string& request)>;

	class Parser {
	public:
		explicit Parser() {
			threadPool.emplace_back([this] {this->worker(); });
			threadPool.emplace_back([this] {this->worker(); });
			for (auto& t : threadPool) {
				if (t.joinable()) {
					t.detach();
				}
			}
		}
		~Parser() {
			stop_flag = true;
		}

		void worker() {
			while (!stop_flag) {		
				std::unique_lock<std::mutex>rdLock(mt);
				cv.wait(rdLock, [this] {return dataReady; });

				parseMessage(requests.front()->reqData);
				requests.pop();

				if (dataReady = true) { //if processed in one thread set dataReady to false for prevention of other thread atempt to parsing this request again.
					dataReady = false;
				}
				cv.notify_one();
			}
		}
		void requestHandler(const std::string &request) {
			std::lock_guard<std::mutex>locker(mt);
			requests.emplace(std::make_unique<Request>(std::move(const_cast<std::string&>(request))));
			dataReady = true;
			cv.notify_one();
		}
	private:
		std::mutex mt;
		std::condition_variable cv;
		bool dataReady = false;

		std::vector<std::thread>threadPool;
		std::atomic_bool stop_flag = ATOMIC_VAR_INIT(false);

		struct Request {
			explicit Request(std::string &&request):reqData(std::forward<std::string>(request)) {};
			std::string reqData;
		};

		std::queue<std::unique_ptr<Request>>requests;

		void parseMessage(std::string& msg) {
			std::cout << msg;
			boost::json::object jObj;
			try {
				jObj = boost::json::parse(msg).as_object();
				std::cout << jObj.at("targetId").as_string();
			}
			catch (boost::system::system_error::exception& msg) {
				//std::cout << msg.what();
			}
		}
	};

	class Server {
	public:
		explicit Server()=default;
		~Server();
		void start();

	private:
		void acceptConnection();
		RequestHandler getReqHandler();

	private:
		unsigned short _port{ 8080 };

		Parser _parser{};
		ba::io_service _io_svc;
		tcp::socket _socket{ _io_svc };
		tcp::acceptor _a{ _io_svc,boost::asio::ip::tcp::endpoint {{},_port} };

		struct ClientSocketHandler : std::enable_shared_from_this<ClientSocketHandler> {
		public:
			ClientSocketHandler(tcp::socket&& sock, RequestHandler handler) :_sock(boost::move(sock)), _reqHandler(handler) {}
			~ClientSocketHandler(){};

			void handle()  {
				auto self = shared_from_this();//for increase live time of current obj created as shared ptr in Server Class method
				_sock.async_read_some(ba::buffer(_buffer), [self, this](boost::system::error_code ec, size_t bytesTransfered) {
					if (ec == boost::asio::error::connection_reset || ec==boost::asio::error::eof)
					{
						_sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
						_sock.close();
						return;
					}
					else if (!ec) {
						totalBytesTransfered += bytesTransfered; //counting arrived bytes
						request.reserve(totalBytesTransfered);   //allocate new memory for it
						copy(_buffer.begin(), _buffer.begin()+ bytesTransfered, back_inserter(request)); //copy all data from buffer

						_buffer.fill(0); //clear buffer before new write from socket

						handle(); //call it again and waiting for new tcp package 
					}
					if (bytesTransfered < 1024) { //when all the data arrived
						request.shrink_to_fit(); //cut all trash bytes
						_reqHandler(request);
						
						_buffer.fill(0);         //clear buffer
						totalBytesTransfered = 0; //set income bytes counter to deffault 
					}
					});
			}
		private:
			tcp::socket _sock;
			size_t totalBytesTransfered{0};
			std::array<char, 1024> _buffer;
			std::string request{""};

		private:
			RequestHandler _reqHandler;
		};
	};
};

namespace InterProcess {
	namespace bipc = boost::interprocess;

	struct Data {             //Data storage for sending to another proccess
		Data(){}
		Data(Data &&other) noexcept {
			_targetId.clear();
			memset(&_targetInfo, 0, sizeof(targetInfo));
			memset(&_browserData, 0, sizeof(BrowserData));

			_targetId = other._targetId;
			_targetInfo= other._targetInfo;
			_browserData = other._browserData;

			memset(&other, 0, sizeof(Data));
		}
		std::string _targetId;

		struct targetInfo {
			std::string os;
			std::string resolution;
			std::string hostName;
		}_targetInfo;

		struct BrowserData {
			std::string browserName;
			std::vector<std::tuple<std::string, std::string, std::string>>resources;
		}_browserData;
	};

	class IPCworkDispatcher {
	protected:
		IPCworkDispatcher() {}
	public:
		static std::shared_ptr<IPCworkDispatcher> getInstance() {
			struct make_shared_enabler : public IPCworkDispatcher {};
			static std::shared_ptr<IPCworkDispatcher>_disp = std::make_shared<make_shared_enabler>();
			return _disp;
		}
		void setCallback(std::function<void(const Data&)>fun) {
			callback = fun;
		}
		void start(){
			if (worker != nullptr) {
				return;
			}
			worker = new std::thread(std::bind(&IPCworkDispatcher::workHandler, this));
			worker->detach();
		}
		void stop(){
			stop_flag.store(true, std::memory_order_relaxed);
		}
		void newData(Data&& data){
			std::lock_guard<std::mutex>dataLock(dataMt);
			dataQueue.emplace(std::move(data));
		}
	private:
		void workHandler() {
			while (!stop_flag.load(std::memory_order_relaxed)) {
				if (!dataQueue.empty()) {
					callback(dataQueue.front());
					dataQueue.pop();
				}
			}
		}
	private:
		std::thread* worker=nullptr;
		std::atomic_bool stop_flag = ATOMIC_VAR_INIT(false);

		std::mutex dataMt;
		std::queue<Data>dataQueue;

		std::function<void(const Data&)>callback;
	};

	class IPC {

	public:
		IPC() {
			//bipc::message_queue::remove("msg_queue");
			//msg_queue = std::make_unique<bipc::message_queue>(bipc::create_only, "msg_queue", 100, sizeof(Data));
		}
		~IPC() {
			wd->stop();
		}
		void _init(){
			wd = IPCworkDispatcher::getInstance();
			wd->setCallback(std::bind(&IPC::sendData, this, std::placeholders::_1));
			wd->start();
		}
		void newData(Data&& data) {
			wd->newData(std::move(data));
		}
	private:
		//std::unique_ptr<bipc::message_queue>msg_queue;
		std::shared_ptr<IPCworkDispatcher>wd;
	private:
		void sendData(const Data& data) { // it will be executed in another thread from IPCworkDispatcher

		}
	};
};