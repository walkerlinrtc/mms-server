/*
 * @Author: jbl19860422
 * @Date: 2023-09-24 09:08:26
 * @LastEditTime: 2023-09-24 15:48:21
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\client\http\http_client.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "http_client.hpp"

#include "protocol/http/http_define.h"
#include "protocol/http/http_request.hpp"
#include "protocol/http/http_response.hpp"

#include "spdlog/spdlog.h"
#include "base/network/tcp_socket.hpp"
#include "base/network/tls_socket.hpp"

using namespace mms;
HttpClient::HttpClient(ThreadWorker *worker, bool is_ssl) : is_ssl_(is_ssl) {
    if (is_ssl) {
        conn_ = std::make_shared<TlsSocket>(this, worker);
    } else {
        conn_ = std::make_shared<TcpSocket>(this, worker);
    }
}

HttpClient::~HttpClient() {

}

void HttpClient::on_socket_open(std::shared_ptr<SocketInterface> sock) {
    ((void)(sock));
    return;
}

void HttpClient::on_socket_close(std::shared_ptr<SocketInterface> sock) {
    ((void)(sock));
    return;
}

void HttpClient::set_buffer_size(size_t s) {
    buffer_size_ = s;
}

boost::asio::awaitable<std::shared_ptr<HttpResponse>> HttpClient::do_req(const std::string & ip, uint16_t port, const HttpRequest & req) {
    bool ret = co_await conn_->connect(ip, port);
    if (!ret) {
        co_return nullptr;
    }
    
    std::string req_data = req.to_req_string();
    if (!co_await conn_->send((const uint8_t*)req_data.data(), req_data.size())) {
        co_return nullptr;
    }
    
    std::shared_ptr<HttpResponse> resp = std::make_shared<HttpResponse>(conn_, buffer_size_);
    co_return resp;
}

void HttpClient::close() {
    if (conn_) {
        conn_->close();
    }
}