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

class printer{
public:
  printer(asio::io_context &io)
      : timer_(io, asio::chrono::seconds(1)),
        count_(0) {
    timer_.async_wait(boost::bind(&printer::print, this));
  }

  void print(){
    if (count_ < 5){
      std::cout<< count_<<std::endl;
      ++count_;

      timer_.expires_at(timer_.expiry()+asio::chrono::seconds(1));
      timer_.async_wait(boost::bind(&printer::print, this));
    }
  }

private:
  asio::steady_timer timer_;
  int count_;
};


int main() {
  asio::io_context io;

//  asio::steady_timer t(io, asio::chrono::seconds(1));
//  int count=0;
//  t.async_wait(boost::bind(print,
//                           asio::placeholders::error, &t, &count));
  printer p(io);

  std::cout << "before io.run" << std::endl;
  io.run();
  std::cout << "after io.run" << std::endl;

  return 0;
}