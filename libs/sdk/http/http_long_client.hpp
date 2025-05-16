/*
 * @Author: jbl19860422
 * @Date: 2023-12-26 22:24:33
 * @LastEditTime: 2023-12-27 13:37:59
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\client\http\http_long_client.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <stdint.h>
#pragma once
#include <string>

#include "base/network/tcp_socket.hpp"
#include "protocol/http/http_define.h"
#include "protocol/http/http_request.hpp"
#include "protocol/http/http_response.hpp"

namespace mms {
class ThreadWorker;

class HttpLongClient : public TcpSocket, public SocketInterfaceHandler {
public:
    HttpLongClient(ThreadWorker *worker) : TcpSocket(this, worker) {
    }

    virtual ~HttpLongClient() {

    }

    void on_socket_open(std::shared_ptr<SocketInterface> sock) {
        ((void)sock);
        return;
    }

    void on_socket_close(std::shared_ptr<SocketInterface> sock) {
        ((void)sock);
        return;
    }

public:
    boost::asio::awaitable<std::shared_ptr<HttpResponse>> do_req(const HttpRequest & req) {
        std::string req_data = req.to_req_string();
        if (!co_await send((const uint8_t*)req_data.data(), req_data.size())) {
            co_return nullptr;
        }
        std::shared_ptr<HttpResponse> resp = std::make_shared<HttpResponse>(shared_from_this());
        co_return resp;
    }

private:
    uint32_t idle_timeout_ = 10000;//最大空闲时间(ms)，超过则关闭
};
};