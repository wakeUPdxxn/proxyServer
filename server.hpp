#include <iostream>
#include <string>
#include <boost/asio.hpp>

namespace ServerSide {
	namespace ba = boost::asio;
	using ba::ip::tcp;
	using err_c = boost::system::error_code;

	class Server {
	public:
		Server(unsigned short port);
		~Server();
		void start();
	private:
		struct MessageHandler : std::enable_shared_from_this<MessageHandler> {
			MessageHandler(tcp::socket&& sock):_sock(std::move(sock)) {}
			void handle() {
				auto handler = shared_from_this();
				ba::async_read(_sock, _buffer, [this, handler](err_c ec, size_t) {
					std::istream in(&_buffer);
					parseMessage(in);
					});
			}
		private:
			tcp::socket _sock;
			ba::streambuf _buffer;
			void parseMessage(std::istream& request);
		};
		void acceptConnection();
		unsigned short _port{ 0 };
		ba::io_service _io_svc;
		tcp::acceptor _a{ _io_svc,boost::asio::ip::tcp::endpoint {{},_port} };
		tcp::socket _sock{ _io_svc };
	};
};

namespace InterProcess {

};