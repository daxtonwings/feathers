cmake_minimum_required(VERSION 3.8)
project(demo-asio)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ./bin)

add_executable(timer01      timer01.cpp)
add_executable(timer02      timer02.cpp)
add_executable(timer03      timer03.cpp)

include_directories(../../extern/asio/asio/include)
include_directories(../../extern/boost_1_81_0)

add_definitions(-DASIO_HAS_BOOST_BIND)
