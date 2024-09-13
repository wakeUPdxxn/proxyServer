#include "server.hpp"

int main() {
	ServerSide::Server proxyServ(2323);
	proxyServ.start();
}