FROM ubuntu:latest
MAINTAINER wakeUPdxxn

RUN apt-get -qq update && \
    apt-get -qq install -y wget g++ make binutils cmake && \
	apt-get -qq install -y libpthread-stubs0-dev libboost-system-dev libboost-json-dev libboost-exception-dev && \
    wget https://github.com/wakeUPdxxn/proxyServer.git

WORKDIR /usr/src/proxyServer
COPY CMakeLists.txt ./
COPY *.cpp ./
COPY *.hpp ./

RUN cmake . && \
    cmake --build . -j$(nproc) && \
    cmake --install .

CMD ["proxyServer"]