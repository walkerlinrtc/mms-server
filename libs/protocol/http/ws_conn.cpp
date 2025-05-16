#include "ws_conn.hpp"
#include "protocol/http/http_request.hpp"
#include "websocket/websocket_packet.hpp"

using namespace mms;
WsConn::WsConn(std::shared_ptr<SocketInterface> tcp_sock) : SocketInterface(nullptr), tcp_sock_(tcp_sock) {

}

WsConn::~WsConn() {

}

ThreadWorker *WsConn::get_worker() {
    return tcp_sock_->get_worker();
}

boost::asio::awaitable<bool> WsConn::connect(const std::string & ip, uint16_t port, int32_t timeout_ms) {
    co_return co_await tcp_sock_->connect(ip, port, timeout_ms);
}

boost::asio::awaitable<bool> WsConn::send(const uint8_t *data, size_t len, int32_t timeout_ms) {
    co_return co_await tcp_sock_->send(data, len, timeout_ms);
}

boost::asio::awaitable<bool> WsConn::send(std::vector<boost::asio::const_buffer> & bufs, int32_t timeout_ms) {
    co_return co_await tcp_sock_->send(bufs, timeout_ms);
}

boost::asio::awaitable<bool> WsConn::recv(uint8_t *data, size_t len, int32_t timeout_ms) {
    co_return co_await tcp_sock_->recv(data, len, timeout_ms);
}

boost::asio::awaitable<int32_t> WsConn::recv_some(uint8_t *data, size_t len, int32_t timeout_ms) {
    co_return co_await tcp_sock_->recv_some(data, len, timeout_ms);
}

void WsConn::open() {
    return;
}

void WsConn::close() {
    if (tcp_sock_) {
        tcp_sock_->close();
    }
    return;
}
