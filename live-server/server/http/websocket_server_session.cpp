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
WebSocketServerSession::WebSocketServerSession(std::shared_ptr<SocketInterface> sock):Session(sock->get_worker()), sock_(sock) {
    set_session_type("websocket");
}

WebSocketServerSession::~WebSocketServerSession() {
}

void WebSocketServerSession::service() {
    auto self(shared_from_this());
    boost::asio::co_spawn(sock_->get_worker()->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        // co_await sock_->cycle_recv([this](const char *buf, int32_t len)->boost::asio::awaitable<std::pair<bool,int32_t>> {
        //     int32_t total_consumed = 0;
        //     int32_t consumed = 0;
        //     do {
        //         if (!packet_) {
        //             packet_ = std::make_shared<WebSocketPacket>();
        //         }
        //         consumed = packet_->decode(buf, len);
        //         total_consumed += consumed;
        //         len -= consumed;
        //     } while(consumed > 0 && len > 0);
        //     co_return std::make_pair(true, total_consumed);
        // });
        co_return;
    }, boost::asio::detached);
}

void WebSocketServerSession::close() {
    if (sock_) {
        sock_->close();
    }
}