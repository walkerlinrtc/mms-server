/*
 * @Author: jbl19860422
 * @Date: 2023-08-27 09:48:03
 * @LastEditTime: 2023-08-27 10:41:08
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\base\network\udp_socket.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <sys/socket.h>
#include <arpa/inet.h>

#include <memory>
#include <boost/array.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/redirect_error.hpp>

namespace mms {
class UdpSocket;
class UdpSocketHandler {
public:
    virtual boost::asio::awaitable<void> on_udp_socket_recv(UdpSocket *sock, std::unique_ptr<uint8_t[]> data, size_t len, boost::asio::ip::udp::endpoint &remote_ep) = 0;
};

class UdpSocket {
public:
    UdpSocket(UdpSocketHandler *handler, std::unique_ptr<boost::asio::ip::udp::socket> sock);
    UdpSocket(std::unique_ptr<boost::asio::ip::udp::socket> sock);

    boost::asio::awaitable<bool> send_to(uint8_t *data, size_t len, const boost::asio::ip::udp::endpoint & remote_ep);
    boost::asio::awaitable<int32_t> recv_from(uint8_t *data, size_t len, boost::asio::ip::udp::endpoint & remote_ep);
    bool bind(const std::string & ip, uint16_t port);
private:
    UdpSocketHandler *handler_ = nullptr;
    std::unique_ptr<boost::asio::ip::udp::socket> sock_;
};
};