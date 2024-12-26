# proxyServer

An tcp server to receive data and notify another process

# Requirements
boost::asio <br />
boost::json <br />
boost::system <br />

# Build
- mkdir build && cd build
- cmake .. -G -Ninja ..
- ninja

# ToDo
- [x] 1)Func for writing data from a message to an file;
- [x] 2)Interprocess messaging mechanism to notify another process
