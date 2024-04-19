//
#include <iostream>
#include <asio.hpp>
#include <boost/bind/bind.hpp>

using asio::ip::tcp;

enum
{
  max_length = 1024
};


class client : public std::enable_shared_from_this<client>
{
public:
  client(asio::io_context &io_context, const std::string& host,
         const std::string& port)
      : socket_(io_context),
        timer_(io_context, asio::chrono::seconds(1))
  {
    tcp::resolver resolver(io_context);
    asio::connect(socket_, resolver.resolve(host, port));
    msg_sn_ = 0;
    pre_sn_ = 0;
    count_ = 0;
  }

  void start()
  {
    do_write();
    timer_.async_wait([this](std::error_code ec) { print(); });
  }

  void print()
  {
    if (count_ < 10) {
      int dsn = msg_sn_ - pre_sn_;
      ++count_;

      pre_sn_ = msg_sn_;
      std::cout << "Timer: " << count_ << ", " << msg_sn_ << "-" << pre_sn_
                << " = " << dsn <<", " << 1e6/dsn << " us"<< std::endl;

      timer_.expires_at(timer_.expiry() + asio::chrono::seconds(1));
//      timer_.async_wait(boost::bind(&client::print, this));
      timer_.async_wait([this](std::error_code ec) { print(); });
    }
  }

  void do_write()
  {
    auto self(shared_from_this());
    char buf[max_length];
    snprintf(buf, max_length, "%08x", msg_sn_);
    send_ = buf;
    ++msg_sn_;
    send_len_ = send_.length();

    asio::async_write(
        socket_,
        asio::buffer(send_, send_len_),
        [this, self](std::error_code ec, std::size_t) {
          if (!ec) {
            do_read();
          }
        });
  }

  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(
        asio::buffer(data_, max_length),
        [this, self](std::error_code ec, std::size_t length) {
//          if (strcmp(data_, send_.c_str())) {
//            std::cout << send_ << "," << data_ << std::endl;
//          }

          if (msg_sn_ < 300000) {
            do_write();
          }
        });
  }

private:
  tcp::socket socket_;
  int msg_sn_;
  int pre_sn_;
  char data_[max_length]{""};

  std::string send_;
  size_t send_len_{0};

  asio::steady_timer timer_;
  int count_;
};

template <typename T>
typename std::enable_if<asio::is_dynamic_buffer_v2<T>::value>::type
SFINAE_test(const T& value)
{
  std::cout<<"T is is_dynamic_buffer_v2"<<std::endl;
}

template <typename T>
typename std::enable_if<asio::is_const_buffer_sequence<T>::value>::type
SFINAE_test(T value)
{
  std::cout<<"T is is_const_buffer_sequence"<<std::endl;
}

template<typename T>
int foo_func(
    const T &buffers,
    typename asio::constraint<asio::is_const_buffer_sequence<T>::value>::type= 0)
{
  return 0;
}


int main()
{
  std::string foo;
  char bar[256];
  SFINAE_test(asio::buffer(bar));

  try {
    asio::io_context io_ctx;
    auto c1 = std::make_shared<client>(io_ctx, "localhost", "7654");
//    auto c2 = std::make_shared<client>(io_ctx, "localhost", "7654");
//    auto c3 = std::make_shared<client>(io_ctx, "localhost", "7654");
//    auto c4 = std::make_shared<client>(io_ctx, "localhost", "7654");
//    auto c5 = std::make_shared<client>(io_ctx, "localhost", "7654");
//    auto c6 = std::make_shared<client>(io_ctx, "localhost", "7654");
//    auto c7 = std::make_shared<client>(io_ctx, "localhost", "7654");
//    auto c8 = std::make_shared<client>(io_ctx, "localhost", "7654");
    c1->start();
//    c2->start();
//    c3->start();
//    c4->start();
//    c5->start();
//    c6->start();
//    c7->start();
//    c8->start();
    io_ctx.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}