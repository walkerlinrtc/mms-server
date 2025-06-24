/*
 * @Author: jbl19860422
 * @Date: 2023-08-27 09:48:03
 * @LastEditTime: 2023-09-16 13:32:24
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\stun\stun_server.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "spdlog/spdlog.h"
#include "stun_server.hpp"
#include "protocol/stun_binding_error_response_msg.hpp"
#include "protocol/stun_binding_response_msg.hpp"
#include "protocol/stun_mapped_address_attr.h"

using namespace mms;

boost::asio::awaitable<void> StunServer::on_udp_socket_recv(UdpSocket *sock, std::unique_ptr<uint8_t[]> data, size_t len, boost::asio::ip::udp::endpoint &remote_ep)
{
    StunMsg stun_msg;
    int32_t ret = stun_msg.decode(data.get(), len);
    if (0 != ret) {
        spdlog::error("decode stun message failed, code:{}", ret);
        co_return; 
    }

    switch(stun_msg.type()) {
        // 这里处理，只是为了让客户端可以获取到它的外网ip地址和端口
        case STUN_BINDING_REQUEST : {
            co_await process_bind_msg(stun_msg, sock, remote_ep);
            break;
        }
    } 
    co_return;
}

boost::asio::awaitable<void> StunServer::process_bind_msg(StunMsg &msg, UdpSocket *sock, const boost::asio::ip::udp::endpoint &remote_ep)
{
    StunBindingResponseMsg binding_resp(msg);
    auto mapped_addr_attr = std::make_unique<StunMappedAddressAttr>(remote_ep.address().to_v4().to_uint(), remote_ep.port());
    binding_resp.add_attr(std::move(mapped_addr_attr));
    auto size = binding_resp.size();
    std::unique_ptr<uint8_t[]> data = std::unique_ptr<uint8_t[]>(new uint8_t[size]);
    int32_t consumed = binding_resp.encode(data.get(), size);
    if (consumed < 0) { // todo:add log
        co_return;
    }

    if (!(co_await sock->send_to(data.get(), size, remote_ep))) { // todo log error
        spdlog::error("send bind response failed");
    }
    co_return;
}
