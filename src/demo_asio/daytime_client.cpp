//
#include <iostream>
#include <boost/array.hpp>
#include <asio.hpp>

using asio::ip::tcp;

int main(int argc, char* argv[]) {
  try{
    if (argc != 2){
      std::cerr << "Usage: daytime_client <host>" << std::endl;
      return 1;
    }

    asio::io_context io_ctx;
    tcp::resolver resolver(io_ctx);
    auto endpoints = resolver.resolve(argv[1], "daytime");

    tcp::socket  socket(io_ctx);
    asio::connect(socket, endpoints);

    for(;;){
      boost::array<char, 128> buf;
      asio::error_code error;

      size_t len = socket.read_some(asio::buffer(buf), error);
      if(error == asio::error::eof){
        break;
      }else if(error){
        throw asio::system_error(error);
      }

      std::cout.write(buf.data(), len);
    }

  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
