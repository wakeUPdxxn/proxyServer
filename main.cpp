#include "server.hpp"

int main() {
	ServerSide::Server proxyServ(2323);
	std::cout << &std::thread::get_id;
	proxyServ.start();
}