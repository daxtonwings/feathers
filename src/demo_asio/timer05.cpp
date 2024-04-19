//
#include <iostream>
#include <asio.hpp>
#include <boost/bind/bind.hpp>

class printer {
public:
  printer(asio::io_context &io)
      : strand_(asio::make_strand(io)),
        timer1_(io, asio::chrono::seconds(1)),
        timer2_(io, asio::chrono::seconds(1)),
        count_(0) {
//    timer_.async_wait(boost::bind(&printer::print, this));
//    timer1_.async_wait(boost::bind(asio::bind_executor(
    timer1_.async_wait(asio::bind_executor(
        strand_,
        boost::bind(&printer::print1, this)));

//    timer2_.async_wait(boost::bind(asio::bind_executor(
    timer2_.async_wait(asio::bind_executor(
        strand_,
        boost::bind(&printer::print2, this)));
  }

  void print1() {
    if (count_ < 10) {
      std::cout << "Timer1:" << count_ << std::endl;
      ++count_;

      timer1_.expires_at(timer1_.expiry() + asio::chrono::seconds(1));
//      timer1_.async_wait(boost::bind(asio::bind_executor(
      timer1_.async_wait(asio::bind_executor(
          strand_,
          boost::bind(&printer::print1, this)));
    }
  }

  void print2() {
    if (count_ < 10) {
      std::cout << "Timer2:" << count_ << std::endl;
      ++count_;

      timer2_.expires_at(timer2_.expiry() + asio::chrono::seconds(1));
//      timer2_.async_wait(boost::bind(asio::bind_executor(
      timer2_.async_wait(asio::bind_executor(
          strand_,
          boost::bind(&printer::print2, this)));
    }
  }

private:
  asio::strand<asio::io_context::executor_type > strand_;
  asio::steady_timer timer1_;
  asio::steady_timer timer2_;
  int count_;
};


int main() {
  asio::io_context io;
  printer p(io);

  asio::thread t(boost::bind(&asio::io_context::run, &io));

  std::cout << "before io.run" << std::endl;
  io.run();
  std::cout << "after io.run" << std::endl;
  t.join();

  return 0;
}