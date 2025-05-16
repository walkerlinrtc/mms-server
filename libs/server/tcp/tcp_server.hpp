#pragma once
#include <memory>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>

#include "base/thread/thread_pool.hpp"
#include "base/network/tcp_socket.hpp"

namespace this_coro = boost::asio::this_coro;
using namespace boost::asio::experimental::awaitable_operators;
using namespace std::literals::chrono_literals;

namespace mms {
class TcpServer {
public:
    TcpServer(SocketInterfaceHandler *tcp_handler, ThreadWorker *worker);
    virtual ~TcpServer();
public:
    virtual int32_t start_listen(uint16_t port, const std::string & addr = "");
    virtual void stop_listen();
    void set_socket_inactive_timeout_ms(uint32_t ms);
private:
    uint16_t port_;
    uint32_t socket_inactive_timeout_ms_ = 10000;
    ThreadWorker *worker_;
    std::atomic<bool> running_{false};
    boost::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    SocketInterfaceHandler * tcp_handler_;
};
};