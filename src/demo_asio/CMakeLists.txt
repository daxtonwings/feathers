cmake_minimum_required(VERSION 3.8)
project(feathers)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ./bin)

add_executable(timer01      timer01.cpp)
add_executable(timer02      timer02.cpp)
add_executable(timer03      timer03.cpp)
add_executable(timer04      timer04.cpp)
add_executable(timer05      timer05.cpp)

add_executable(daytime_client   daytime_client.cpp)
add_executable(daytime_server   daytime_server.cpp)
add_executable(daytime_aserver  daytime_aserver.cpp)

add_executable(async_tcp_echo_server        11_async_tcp_echo_server.cpp)
add_executable(async_tcp_echo_client        11_async_tcp_echo_client.cpp)
add_executable(blocking_tcp_echo_client     blocking_tcp_echo_client.cpp)

include_directories(../../extern/asio/asio/include)
include_directories(../../extern/boost_1_81_0)

add_definitions(-DASIO_HAS_BOOST_BIND)

add_definitions(-DASIO_NO_DYNAMIC_BUFFER_V1)
#add_definitions(-DASIO_HAS_VARIADIC_TEMPLATES)
