cmake_minimum_required(VERSION 3.8)
project(feathers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ./bin)

add_executable(demo             demo.cpp)


#include_directories(../../extern/asio/asio/include)
include_directories(../../extern/boost_1_81_0)
#add_definitions(-DASIO_HAS_BOOST_BIND)
