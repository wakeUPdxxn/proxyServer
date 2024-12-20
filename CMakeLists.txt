cmake_minimum_required (VERSION 3.2)
project(proxyServer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(-DCMAKE_TOOLCHAIN_FILE="C:/Work/libs/vcpkg/scripts/buildsystems/vcpkg.cmake") #temp flag
set(Boost_USE_MULTITHREADED ON)
set(SRC server.cpp main.cpp fileManager.cpp)
set(HEADERS server.hpp ipc.hpp fileManager.hpp)

find_package(Boost COMPONENTS json REQUIRED )
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

add_executable(proxyServer ${SRC} ${HEADERS})
target_link_libraries (proxyServer LINK_PUBLIC ${Boost_LIBRARIES})
