# proxyServer

An tcp server to receive data and notify another process

# Requirements
- Boost asio <br />
- Boost json <br />
- Boost system <br />

# Build
```mkdir build && cd build```</br>
```cmake .. -G -Ninja ..```</br>
``` ninja ```

# ToDo
- [x] 1)Func for writing data from a message to an file;
- [x] 2)Interprocess messaging mechanism to notify another process
