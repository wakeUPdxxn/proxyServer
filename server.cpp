#include "server.hpp"

using namespace ServerSide;

Server::Server(unsigned short port) : _port(port) {}

Server::~Server(){
    _a.close();
    _io_svc.stop();
}

void ServerSide::Server::start(){
    _a.listen();
    acceptConnection();
    _io_svc.run();
}



void Server::acceptConnection() {
    _a.async_accept(_sock, [this](err_c ec) {
        if (!ec) {
            std::make_shared<MessageHandler>(std::move(_sock))->handle();
            acceptConnection();
        }
        });
}

void ServerSide::Server::MessageHandler::parseMessage(std::istream& request){
    //Messages::BrowserData::ParseFromIstream(request);
    std::cout << request.rdbuf();
}
