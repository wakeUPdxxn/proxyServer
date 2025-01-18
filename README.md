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
- [x] Func for writing data from a message to an file;
- [x] Interprocess messaging mechanism to notify another process
- [ ] Logging mechanism
