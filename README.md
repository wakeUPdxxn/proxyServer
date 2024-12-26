# proxyServer

An tcp server for receiving data and transfering state to another process
# Requirements
boost::json
# Build
- mkdir build && cd build
- cmake .. -G -GNinja ..
- ninja
# ToDo

- [x] 1)Func for writing data from a message to an file;
- [x] 2)Interprocess messaging mechanism to notify another process
