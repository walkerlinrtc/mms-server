#include "tcp_server.hpp"

#include <string>

#include "spdlog/spdlog.h"

using namespace mms;

TcpServer::TcpServer(SocketInterfaceHandler *tcp_handler, ThreadWorker *worker)
    : worker_(worker), tcp_handler_(tcp_handler) {}

TcpServer::~TcpServer() {}

int32_t TcpServer::start_listen(uint16_t port, const std::string &addr) {
    if (!worker_) {
        return -1;
    }

    running_ = true;
    port_ = port;
    // 使用协程异步监听并处理新连接
    boost::asio::co_spawn(
        worker_->get_io_context(), ([port, addr, this]() -> boost::asio::awaitable<void> {
            // 创建监听地址端点
            boost::asio::ip::tcp::endpoint endpoint;
            endpoint.port(port);
            if (!addr.empty()) {
                endpoint.address(boost::asio::ip::make_address(addr));
            } else {
                endpoint.address(boost::asio::ip::make_address("0.0.0.0"));
            }

            acceptor_ = std::make_shared<boost::asio::ip::tcp::acceptor>(worker_->get_io_context(), endpoint);
            acceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            while (1) {
                boost::system::error_code ec;
                // 随机取个worker
                auto worker = thread_pool_inst::get_mutable_instance().get_worker(-1);
                // 在新worker中接收新连接并处理
                auto tcp_sock = co_await acceptor_->async_accept(
                    worker->get_io_context(), boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (!ec) {
                    std::shared_ptr<TcpSocket> tcp_socket =
                        std::make_shared<TcpSocket>(tcp_handler_, std::move(tcp_sock), worker);
                    tcp_socket->set_socket_inactive_timeout_ms(socket_inactive_timeout_ms_);
                    // 启动对该连接的处理
                    tcp_socket->open();
                } else {
                    if (!running_) {
                        co_return;
                    }
                    boost::asio::steady_timer timer(co_await this_coro::executor);
                    timer.expires_after(100ms);
                    co_await timer.async_wait(boost::asio::use_awaitable);
                }
            }
            co_return;
        }),
        [this](std::exception_ptr exp) {
            ((void)exp);
            return -2;
        });
    return 0;
}

void TcpServer::stop_listen() {
    // 设置停止标志
    running_ = false;
    // 在worker线程中执行清理
    worker_->dispatch([this] {
        if (acceptor_) {
            acceptor_->close(); // 关闭acceptor
            acceptor_.reset();  // 释放智能指针
        }
    });
}

void TcpServer::set_socket_inactive_timeout_ms(uint32_t ms) { socket_inactive_timeout_ms_ = ms; }