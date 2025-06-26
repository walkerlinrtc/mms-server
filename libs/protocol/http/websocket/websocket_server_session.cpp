/*
 * @Author: jbl19860422
 * @Date: 2023-12-27 10:17:17
 * @LastEditTime: 2023-12-27 21:32:40
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\http\websocket_server_session.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include "websocket_server_session.hpp"
#include "websocket/websocket_packet.hpp"
#include "base/network/socket_interface.hpp"
#include "base/thread/thread_worker.hpp"

using namespace mms;
WebSocketServerSession::WebSocketServerSession(std::shared_ptr<SocketInterface> sock):Session(sock->get_worker()), sock_(sock), wg_(sock->get_worker()) {
    set_session_type("websocket");
    recv_buf_ = std::make_unique<char[]>(max_recv_buf_bytes_);
}

WebSocketServerSession::~WebSocketServerSession() {
}

void WebSocketServerSession::start() {
    auto self(shared_from_this());
    wg_.add(1);
    boost::asio::co_spawn(sock_->get_worker()->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        while (1) {
            if (max_recv_buf_bytes_ - recv_buf_bytes_ <= 0) {
                spdlog::error("no enough buffer for websocket");
                co_return;
            }

            auto s = co_await sock_->recv_some((uint8_t*)recv_buf_.get() + recv_buf_bytes_, max_recv_buf_bytes_ - recv_buf_bytes_);
            if (s < 0) {
                co_return;
            }
            recv_buf_bytes_ += s;

            while (1) {
                auto consumed = co_await process_recv_buffer();
                if (consumed == 0) {// 0 表示不够数据
                    break;
                } else if (consumed < 0) {
                    co_return;
                }

                // 解析完成，移动buffer
                auto left_size = recv_buf_bytes_ - consumed;
                std::memmove((char*)recv_buf_.get(), recv_buf_.get() + consumed, left_size);
                recv_buf_bytes_ = left_size;
            }
        }
        co_return;
    }, [this, self](std::exception_ptr exp) {
        (void)exp;
        wg_.done();
    });
}

boost::asio::awaitable<int32_t> WebSocketServerSession::process_recv_buffer() {
    int32_t total_consumed = 0;
    int32_t consumed = 0;
    auto len = recv_buf_bytes_;
    do {
        if (!packet_) {
            packet_ = std::make_shared<WebSocketPacket>();
        }
        consumed = packet_->decode((char*)recv_buf_.get(), len);
        total_consumed += consumed;
        len -= consumed;
    } while(consumed > 0 && len > 0);
    co_return total_consumed;
}

void WebSocketServerSession::stop() {
    auto self(shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        if (sock_) {
            sock_->close();
        }
        co_await wg_.wait();
        co_return;
    }, boost::asio::detached);
}