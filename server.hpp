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

using RequestHandler = std::function<void(std::istream& in)>;

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
				parseMessage(requests.front().data); 
				if (dataReady = true) {
					dataReady = false;
				}
				cv.notify_one();
			}
		}
		void requestHandler(std::istream& in) {
			Request request;
			request.data.assign(std::istream_iterator<char>(in), {});

			std::unique_lock<std::mutex>locker(mt);
			requests.push(request);		
			dataReady = true;
			cv.notify_one();
		}
	private:
		std::mutex mt;
		std::condition_variable cv;
		bool dataReady = false;

		std::vector<std::thread>threadPool;

		struct Request {
			std::vector<char>data{};
		};

		std::queue<Request>requests;

		void parseMessage(vector<char>& msg) {
			for (auto n : msg) {
				std::cout << n;
			}
			requests.pop();
		}
	};

	class Server {
	public:
		Server(unsigned short port);
		~Server();
		void start();

	private:
		struct ClientSocketHandler : std::enable_shared_from_this<ClientSocketHandler> {
			ClientSocketHandler(tcp::socket&& sock,RequestHandler handler) :_sock(boost::move(sock)),_reqHandler(handler) {}
			void handle() {
				auto self = shared_from_this();//for increase live time of current obj created as shr ptr in Server Class method
				ba::async_read(_sock,_buffer, [self, this](err_c ec, size_t bytes) {
					if (ec == boost::asio::error::connection_reset)
					{
						_sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
						return;
					}
					else if (!ec || ec == boost::asio::error::eof) {
						std::istream in(&_buffer);
						_reqHandler(in);
						handle();
					}
					else {
						std::cout << ec.what();
					}
					});
			}
		private:
			tcp::socket _sock;
			ba::streambuf _buffer;
			RequestHandler _reqHandler;
		};

		unsigned short _port{ 2323 };

		Parser _parser{};
		ba::io_service _io_svc;
		tcp::socket _socket{ _io_svc };
		tcp::acceptor _a{ _io_svc,boost::asio::ip::tcp::endpoint {{},_port} };

	private:
		void acceptConnection();
		RequestHandler getReqHandler();

	};
};

namespace InterProcess {

};