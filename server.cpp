#include "server.hpp"

namespace bpt = boost::property_tree;
using namespace ServerSide;

Server::Server(unsigned short port) :_port(port) {}

Server::~Server(){
    _a.close();
    _io_svc.stop();
}

void Server::start(){
    _a.listen();
    acceptConnection();
    _io_svc.run();
}

void ServerSide::Server::acceptConnection() {
    _a.async_accept(_socket, [this](err_c ec) {
        if (!ec) {
            try {
                std::make_shared<ClientSocketHandler>(boost::move(_socket), getReqHandler())->handle();
                acceptConnection();
            }
            catch (std::exception& e) {
                std::cout << e.what();
            }
        }
        });
}

RequestHandler ServerSide::Server::getReqHandler(){
    std::function<void(string& request)> handler = [&](std::string& request) {
        _parser.requestHandler(request);
    };
    return handler;
}

