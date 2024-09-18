cmake_minimum_required (VERSION 3.2)
project(proxyServer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(-DCMAKE_TOOLCHAIN_FILE="C:/Work/libs/vcpkg/scripts/buildsystems/vcpkg.cmake") #temp flag
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(Boost_USE_MULTITHREADED ON)

find_package(Boost REQUIRED)

IF (Boost_FOUND)
    INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
    ADD_DEFINITIONS( "-DHAS_BOOST" )
ENDIF()


add_executable(proxyServer server.cpp main.cpp server.hpp)
target_link_libraries (proxyServer ${Boost_LIBRARIES})
