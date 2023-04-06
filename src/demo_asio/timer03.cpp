//
#include <iostream>
#include <asio.hpp>
#include <boost/bind/bind.hpp>

void print(const asio::error_code &,
           asio::steady_timer* t, int* count) {
  if(*count < 5){
    std::cout << "Hello, world!" << std::endl;
    ++(*count);
    t->expires_at(t->expiry() + asio::chrono::seconds(1));
    t->async_wait(boost::bind(print,
                              asio::placeholders::error, t, count));
  }
}

int main() {
  asio::io_context io;

  asio::steady_timer t(io, asio::chrono::seconds(1));
//  t.wait();
//  std::cout << "Hello, world!" << std::endl;
  int count=0;
  t.async_wait(boost::bind(print,
                           asio::placeholders::error, &t, &count));
  std::cout << "before io.run" << std::endl;
  io.run();
  std::cout << "after io.run" << std::endl;

  return 0;
}