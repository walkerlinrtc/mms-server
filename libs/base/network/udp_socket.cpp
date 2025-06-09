#include "udp_socket.hpp"
using namespace mms;
UdpSocket::UdpSocket(UdpSocketHandler *handler,
                     std::unique_ptr<boost::asio::ip::udp::socket> sock)
    : handler_(handler), sock_(std::move(sock)) {}

UdpSocket::UdpSocket(std::unique_ptr<boost::asio::ip::udp::socket> sock)
    : sock_(std::move(sock)) {}

boost::asio::awaitable<bool>
UdpSocket::send_to(uint8_t *data, size_t len,
                   const boost::asio::ip::udp::endpoint &remote_ep) {
  boost::system::error_code ec;
  auto size = co_await sock_->async_send_to(
      boost::asio::buffer(data, len), remote_ep,
      boost::asio::redirect_error(boost::asio::use_awaitable, ec));
  if (ec || size != len) {
    co_return false;
  }
  co_return true;
}

boost::asio::awaitable<int32_t>
UdpSocket::recv_from(uint8_t *data, size_t len,
                     boost::asio::ip::udp::endpoint &remote_ep) {
  boost::system::error_code ec;
  auto size = co_await sock_->async_receive_from(
      boost::asio::buffer(data, len), remote_ep,
      boost::asio::redirect_error(boost::asio::use_awaitable, ec));
  if (ec) {
    co_return -1;
  }
  co_return size;
}

bool UdpSocket::bind(const std::string &ip, uint16_t port) {
  boost::asio::ip::udp::endpoint local_ep(boost::asio::ip::make_address(ip),
                                          port);
  sock_->open(boost::asio::ip::udp::v4());
  sock_->set_option(boost::asio::ip::udp::socket::reuse_address(true));
  boost::system::error_code ec;
  sock_->bind(local_ep, ec);
  if (ec) {
    return false;
  }
  return true;
}