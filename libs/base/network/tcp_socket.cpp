#include <iostream>
#include <span>
#include <variant>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include "base/utils/utils.h"
#include "tcp_socket.hpp"
#include "spdlog/spdlog.h"

using namespace boost::asio::experimental::awaitable_operators;
namespace mms {
TcpSocket::TcpSocket(SocketInterfaceHandler *handler, boost::asio::ip::tcp::socket sock, ThreadWorker *worker) : 
                                                        SocketInterface(handler),
                                                        worker_(worker), 
                                                        socket_(std::move(sock)), 
                                                        connect_timeout_timer_(worker->get_io_context()),
                                                        recv_timeout_timer_(worker->get_io_context()), 
                                                        send_timeout_timer_(worker->get_io_context()),
                                                        active_timeout_timer_(worker->get_io_context())
{
}

TcpSocket::TcpSocket(SocketInterfaceHandler *handler, ThreadWorker *worker):
                                                        SocketInterface(handler), 
                                                        worker_(worker), 
                                                        socket_(worker->get_io_context()), 
                                                        connect_timeout_timer_(worker->get_io_context()),
                                                        recv_timeout_timer_(worker->get_io_context()), 
                                                        send_timeout_timer_(worker->get_io_context()),
                                                        active_timeout_timer_(worker->get_io_context())  
{
}

void TcpSocket::set_socket_inactive_timeout_ms(uint32_t ms) {
    socket_inactive_timeout_ms_ = ms;
}

TcpSocket::~TcpSocket() {
    spdlog::debug("destroy TcpSocket");
}

void TcpSocket::open() {
    auto self(shared_from_this());
    if (handler_) {
        handler_->on_socket_open(self);
    }
    
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        active_time_ = Utils::get_current_ms();
        while (1) {
            active_timeout_timer_.expires_from_now(std::chrono::milliseconds(socket_inactive_timeout_ms_));
            boost::system::error_code ec;
            co_await active_timeout_timer_.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                break;
            }

            auto now = Utils::get_current_ms();
            if (now - active_time_ >= socket_inactive_timeout_ms_) {
                spdlog::debug("tcp socket[{}] active timeout", id_);
                break;
            }
        }
        co_return;
    }, [this, self](std::exception_ptr exp) {
        (void)exp;
        close();
    });
}

void TcpSocket::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }
    
    active_timeout_timer_.cancel();
    if (handler_) {
        handler_->on_socket_close(shared_from_this());
    }
    socket_.close();
}

boost::asio::awaitable<bool> TcpSocket::connect(const std::string & ip, uint16_t port, int32_t timeout_ms) {
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(ip), port);
    boost::system::error_code ec;
    if (timeout_ms > 0) {
        active_timeout_timer_.expires_from_now(std::chrono::milliseconds(timeout_ms));
        auto results = co_await(socket_.async_connect(endpoint, boost::asio::redirect_error(boost::asio::use_awaitable, ec)) ||
                                connect_timeout_timer_.async_wait(boost::asio::use_awaitable));
        if (results.index() == 1) {//超时
            co_return false;
        }
    } else {
        co_await socket_.async_connect(endpoint, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    }
    
    if (ec) {
        co_return false;
    }
    active_time_ = Utils::get_current_ms();
    open();
    co_return true;
}

boost::asio::awaitable<bool> TcpSocket::send(const uint8_t *data, size_t len, int32_t timeout_ms) {
    int32_t send_bytes = 0;
    if (timeout_ms > 0) {
        send_timeout_timer_.expires_from_now(std::chrono::milliseconds(timeout_ms));
        std::variant<bool, std::monostate> results;
        results = co_await(
            [data, len, &send_bytes, this]()->boost::asio::awaitable<bool>{
                boost::system::error_code ec;
                send_bytes = co_await boost::asio::async_write(socket_, boost::asio::buffer(data, len), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    co_return false;
                }
                co_return true;
            }() ||
            send_timeout_timer_.async_wait(boost::asio::use_awaitable)
        );

        if (results.index() == 1) {//超时
            co_return false;
        }

        if (!std::get<0>(results)) {
            co_return false;
        }
    } else {
        boost::system::error_code ec;
        send_bytes = co_await boost::asio::async_write(socket_, boost::asio::buffer(data, len), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) {
            co_return false;
        }
    }
    
    out_bytes_ += send_bytes;
    active_time_ = Utils::get_current_ms();
    co_return true;
}

boost::asio::awaitable<bool> TcpSocket::send(std::vector<boost::asio::const_buffer> & bufs, int32_t timeout_ms) {
    int32_t send_bytes = 0;
    if (timeout_ms > 0) {
        send_timeout_timer_.expires_after(std::chrono::milliseconds(timeout_ms));
        std::variant<bool, std::monostate> results;
        results = co_await(
            [&bufs, &send_bytes, this]()->boost::asio::awaitable<bool>{
                boost::system::error_code ec;
                std::span sbufs(bufs);
                send_bytes = co_await boost::asio::async_write(socket_, bufs, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    co_return false;
                }
                co_return true;
            }() ||
            send_timeout_timer_.async_wait(boost::asio::use_awaitable)
        );

        if (results.index() == 1) {//超时
            co_return false;
        }
        
        if(!std::get<0>(results)) {
            co_return false;
        }
    } else {
        boost::system::error_code ec;
        send_bytes = co_await boost::asio::async_write(socket_, bufs, boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) {
            co_return false;
        }
    }
    
    out_bytes_ += send_bytes;
    active_time_ = Utils::get_current_ms();
    co_return true;
}

boost::asio::awaitable<bool> TcpSocket::recv(uint8_t *data, size_t len, int32_t timeout_ms) {
    if (timeout_ms > 0) {
        recv_timeout_timer_.expires_from_now(std::chrono::milliseconds(timeout_ms));
        std::variant<bool, std::monostate> results;
        results = co_await(
            [data, len, this]()->boost::asio::awaitable<bool> {
                boost::system::error_code ec;
                co_await boost::asio::async_read(socket_, boost::asio::buffer(data, len), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) {
                    co_return false;
                }
                co_return true;
            }() ||
            recv_timeout_timer_.async_wait(boost::asio::use_awaitable)
        );

        if (results.index() == 1) {//超时
            co_return false;//考虑下返回码的问题,error应该需要包装下，但还得考虑性能
        }
        
        if (!std::get<0>(results)) {
            co_return false;
        }
    } else {
        boost::system::error_code ec;
        co_await boost::asio::async_read(socket_, boost::asio::buffer(data, len), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) {
            co_return false;
        }
    }
    
    in_bytes_ += len;
    active_time_ = Utils::get_current_ms();
    co_return true;
}

boost::asio::awaitable<int32_t> TcpSocket::recv_some(uint8_t *data, size_t len, int32_t timeout_ms) {
    boost::system::error_code ec;
    std::size_t s; 
    if (timeout_ms > 0) {
        recv_timeout_timer_.expires_from_now(std::chrono::milliseconds(timeout_ms));
        std::variant<std::size_t, std::monostate> results = co_await (
            socket_.async_read_some(boost::asio::buffer(data, len), boost::asio::redirect_error(boost::asio::use_awaitable, ec)) ||
            recv_timeout_timer_.async_wait(boost::asio::use_awaitable)
        );

        if (results.index() == 1) {//超时
            co_return -1;
        }
        s = std::get<0>(results);
    } else {
        s = co_await socket_.async_read_some(boost::asio::buffer(data, len), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    }

    if (ec) {
        co_return -2;
    }
    active_time_ = Utils::get_current_ms();
    in_bytes_ += s;
    co_return s;
}

std::string TcpSocket::get_local_address() {
    return socket_.local_endpoint().address().to_string();
}

std::string TcpSocket::get_remote_address() {
    return socket_.remote_endpoint().address().to_string();
}

};