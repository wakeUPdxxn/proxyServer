#include "server.hpp"

using namespace ServerSide;

Server::~Server(){
    _a.close();
    _io_svc.stop();
}

void Server::start(){
    _a.listen(MAX_CONNECTIONS);
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
    std::function<void(const std::string &request)> handler = [&](const std::string &request) {
        _parser.requestHandler(request);
    };
    return handler;
}

