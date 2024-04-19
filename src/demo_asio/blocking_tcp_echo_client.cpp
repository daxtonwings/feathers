//
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <asio.hpp>

using asio::ip::tcp;

enum {max_length=1024};

int main(int argc, char* argv[]){
  try{
    char port[] = "7654";

    asio::io_context io_ctx;

    tcp::socket s(io_ctx);
    tcp::resolver  resolver(io_ctx);
    asio::connect(s, resolver.resolve("localhost", port));

    char send_[max_length];
    size_t send_len;
    char data_[max_length];
    size_t data_len;
    int msg_sn = 0;

    while(msg_sn < 10){

      snprintf(send_, max_length, "%08x", msg_sn);
      send_len = strlen(send_);
      asio::write(s, asio::buffer(send_));

      data_len = asio::read(s, asio::buffer(data_, send_len));

      std::cout<<send_<<","<<data_<<std::endl;

      ++ msg_sn;
    }

//    std::string request = "hello";
//    size_t request_len = request.size();
//    asio::write(s, asio::buffer(request));
//
//    char reply[max_length];
//    size_t reply_len = asio::read(s, asio::buffer(reply, request_len));
//
//    std::cout << "reply: ";
//    std::cout.write(reply, reply_len);
//    std::cout << "\n";
  } catch (std::exception& e){
    std::cerr << "Exception: "<< e.what() << "\n";
  }

  return 0;
}