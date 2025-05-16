#pragma once
#include <memory>
#include <vector>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>

#include "base/network/socket_interface.hpp"
namespace mms {
class Session;
class ThreadWorker;
class HttpRequest;
class WsConn : public SocketInterface {
public:
    WsConn(std::shared_ptr<SocketInterface> tcp_sock);
    virtual ~WsConn();
    ThreadWorker *get_worker();
    // socket interface
    void open() override;
    void close() override;
    boost::asio::awaitable<bool> connect(const std::string & ip, uint16_t port, int32_t timeout_ms = 0) override;
    boost::asio::awaitable<bool> send(const uint8_t *data, size_t len, int32_t timeout_ms = 0) override;
    boost::asio::awaitable<bool> send(std::vector<boost::asio::const_buffer> & bufs, int32_t timeout_ms = 0) override;
    boost::asio::awaitable<bool> recv(uint8_t *data, size_t len, int32_t timeout_ms = 0) override;
    boost::asio::awaitable<int32_t> recv_some(uint8_t *data, size_t len, int32_t timeout_ms = 0) override;

    void setReq(std::shared_ptr<HttpRequest> req) {
        req_ = req;
    }

    std::shared_ptr<HttpRequest> getReq() {
        return req_;
    }
private:
    std::shared_ptr<SocketInterface> tcp_sock_;
    std::shared_ptr<HttpRequest> req_;
    // std::shared_ptr<Session> session_;
};
};