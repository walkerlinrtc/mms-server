/*
 * @Author: jbl19860422
 * @Date: 2023-06-11 11:33:02
 * @LastEditTime: 2023-09-16 11:40:19
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\stun\stun_server.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include "base/network/tls_socket.hpp"

#include "server/udp/udp_server.hpp"
#include "server/tcp/tcp_server.hpp"
#include "base/network/udp_socket.hpp"
#include "base/thread/thread_pool.hpp"

#include "protocol/stun_msg.h"

namespace mms {
#define STUN_DEFAULT_PORT 3478
class StunServer : public UdpServer {
public:
    StunServer(ThreadWorker *worker) : UdpServer({worker}){

    };

    virtual ~StunServer() {

    }
public:
    bool start(const std::string & ip, uint32_t port = STUN_DEFAULT_PORT) {
        bool ret = start_listen(ip, port);
        return ret;
    }

    void stop() {
        stop_listen();
    }
private:
    boost::asio::awaitable<void> on_udp_socket_recv(UdpSocket *sock, std::unique_ptr<uint8_t[]> data, size_t len, boost::asio::ip::udp::endpoint &remote_ep);
    boost::asio::awaitable<void> process_bind_msg(StunMsg & msg, UdpSocket *sock, const boost::asio::ip::udp::endpoint & remote_ep);
};
};