#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/move/move.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <queue>
#include <condition_variable>

using namespace std;
namespace ba = boost::asio;
using ba::ip::tcp;
using err_c = boost::system::error_code;

using RequestHandler = std::function<void(std::string& request)>;

namespace ServerSide {

	class Parser {
	public:
		Parser() {
			threadPool.emplace_back([this] {this->worker(); });
			threadPool.emplace_back([this] {this->worker(); });
			for (auto& thrd : threadPool) {
				if (thrd.joinable()) {
					thrd.detach();
				}
			}
		}
		void worker() {
			while (true) {
				std::unique_lock<std::mutex>rdLock(mt);
				cv.wait(rdLock, [this] {return dataReady; });
				parseMessage(requests.front()->reqData); 
				if (dataReady = true) {
					dataReady = false;
				}
				cv.notify_one();
			}
		}
		void requestHandler(std::string& request) {
			std::unique_lock<std::mutex>locker(mt);
			requests.push(new Request(request));
			dataReady = true;
			cv.notify_one();
		}
	private:
		std::mutex mt;
		std::condition_variable cv;
		bool dataReady = false;

		std::vector<std::thread>threadPool;

		struct Request {
			explicit Request(std::string &request):reqData(move(request)) {};
			std::string reqData;
		};

		std::queue<Request*>requests;

		void parseMessage(std::string& msg) {
			std::cout << msg;
			delete requests.front();
			requests.pop();
		}
	};

	class Server {
	public:
		Server(unsigned short port);
		~Server();
		void start();

	private:
		void acceptConnection();
		RequestHandler getReqHandler();

	private:
		unsigned short _port{ 2323 };

		Parser _parser{};
		ba::io_service _io_svc;
		tcp::socket _socket{ _io_svc };
		tcp::acceptor _a{ _io_svc,boost::asio::ip::tcp::endpoint {{},_port} };

		struct ClientSocketHandler : std::enable_shared_from_this<ClientSocketHandler> {
		public:
			ClientSocketHandler(tcp::socket&& sock, RequestHandler handler) :_sock(boost::move(sock)), _reqHandler(handler) {}
			~ClientSocketHandler() = default;

			void handle() {
				auto self = shared_from_this();//for increase live time of current obj created as shr ptr in Server Class method
				_sock.async_read_some(ba::buffer(data), [self, this](boost::system::error_code ec, size_t byt) {
					if (ec == boost::asio::error::connection_reset)
					{
						_sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
						return;
					}
					else if (!ec) {
						copy(data.begin(), data.end(), back_inserter(request));
						handle();
					}
					if (ec == ba::error::eof) {
						_reqHandler(request);
					}
					});
			}
		private:
			std::array<char, 1024> data;
			tcp::socket _sock;
			std::string request;
			RequestHandler _reqHandler;
		};
	};
};

namespace InterProcess {

};