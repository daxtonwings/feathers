//
#include <ctime>
#include <iostream>
#include <asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

using asio::ip::tcp;

std::string make_daytime_string() {
  using namespace std;
  time_t now = time(0);
  return ctime(&now);
}

class tcp_connection
    : public std::enable_shared_from_this<tcp_connection> {
public:
  typedef boost::shared_ptr<tcp_connection> pointer;

  static pointer create(asio::io_context &io_context) {
    return pointer(new tcp_connection(io_context));
  }

  tcp::socket &socket() { return socket_; }

  void start() {
    message_ = make_daytime_string();
    asio::async_write(
        socket_,
        asio::buffer(message_),
        boost::bind(
            &tcp_connection::handle_write,
            shared_from_this(),
            _1, _2));
  }

private:
  tcp::socket socket_;
  std::string message_;

  tcp_connection(asio::io_context &io_context) : socket_(io_context) {}

  void handle_write(const asio::error_code &, size_t) {}

};


class tcp_server {
public:
  tcp_server(asio::io_context &io_ctx)
      : io_context_(io_ctx),
        acceptor_(io_ctx, tcp::endpoint(tcp::v4(), 13)) {
    start_accept();
  };

private:
  void start_accept() {
    tcp_connection::pointer new_connection =
        tcp_connection::create(io_context_);
    acceptor_.async_accept(
        new_connection->socket(),
        boost::bind(&tcp_server::handle_accept, this, new_connection, _1));
  }

  void handle_accept(tcp_connection::pointer new_connection,
                     const asio::error_code &err) {
    if (!err) {
      new_connection->start();
    }
    start_accept();
  }

  asio::io_context &io_context_;
  tcp::acceptor acceptor_;
};


int main() {
  try {
    asio::io_context io_ctx;
    tcp_server server(io_ctx);
    io_ctx.run();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
