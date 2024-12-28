cmake_minimum_required (VERSION 3.20)
project(proxyServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED on)

IF (NOT MSVC)
    set(CMAKE_CXX_COMPILER g++)
    set(CMAKE_C_COMPILER gcc)
ENDIF()

if (UNIX)
   set(THREADS_PREFER_PTHREAD_FLAG ON)
ENDIF()

set(Boost_USE_STATIC_LIBS OFF) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF) 

set(SRC server.cpp main.cpp fileManager.cpp)
set(HEADERS server.hpp ipc.hpp fileManager.hpp)

find_package(Boost REQUIRED COMPONENTS json system)
find_package(Threads REQUIRED)

#enable_testing()
#add_subdirectory(tests)

add_executable(proxyServer ${SRC} ${HEADERS})
target_link_libraries (proxyServer LINK_PUBLIC Threads::Threads)

IF (Boost_FOUND)
    INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
    target_link_libraries (proxyServer LINK_PUBLIC ${Boost_LIBRARIES}) 
ENDIF()

include(GNUInstallDirs)
install(TARGETS proxyServer
          DESTINATION ${CMAKE_INSTALL_BINDIR}
)

