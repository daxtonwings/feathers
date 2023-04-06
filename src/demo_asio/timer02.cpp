//
#include <iostream>
#include <asio.hpp>

void print(const asio::error_code &) {
  std::cout << "Hello, world!" << std::endl;
}

int main() {
  asio::io_context io;

  asio::steady_timer t(io, asio::chrono::seconds(5));
//  t.wait();
//  std::cout << "Hello, world!" << std::endl;
  t.async_wait(&print);
  std::cout << "before io.run" << std::endl;
  io.run();
  std::cout << "after io.run" << std::endl;

  return 0;
}