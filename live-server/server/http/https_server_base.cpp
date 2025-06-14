/*
 * @Author: jbl19860422
 * @Date: 2023-09-16 10:32:17
 * @LastEditTime: 2023-12-06 22:04:43
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\http\https_server_base.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */

#include <boost/shared_ptr.hpp>
#include <memory>
#include <fstream>

#include "log/log.h"
#include "base/thread/thread_pool.hpp"
#include "base/utils/utils.h"
#include "https_server_base.hpp"
#include "http_server_session.hpp"
#include "config/config.h"
#include "ws_conn.hpp"
using namespace mms;

HttpsServerBase::HttpsServerBase(ThreadWorker *w) {
    tls_server_ = std::make_unique<TlsServer>(this, this, w);
}

bool HttpsServerBase::start(uint16_t port) {
    if (!register_route()) {
        return false;
    }

    tls_server_->set_socket_inactive_timeout_ms(Config::get_instance()->get_socket_inactive_timeout_ms());
    auto ret = tls_server_->start_listen(port);
    if (0 != ret) {
        CORE_ERROR("start https server failed, code:{}", ret);
        return false;
    } 
    
    return true;
}

HttpsServerBase::~HttpsServerBase() {

}

void HttpsServerBase::stop() {
    tls_server_->stop_listen();
}

void HttpsServerBase::on_socket_open(std::shared_ptr<SocketInterface> tls_socket) {
    std::shared_ptr<HttpServerSession> s = std::make_shared<HttpServerSession>(this, tls_socket);
    tls_socket->set_session(s);
    s->start();
}

void HttpsServerBase::on_socket_close(std::shared_ptr<SocketInterface> tls_socket) {
    std::shared_ptr<HttpServerSession> s = std::static_pointer_cast<HttpServerSession>(tls_socket->get_session());
    tls_socket->clear_session();
    if (s) {
        s->stop();
    }
}

bool HttpsServerBase::register_route() {
    return true;
}

bool HttpsServerBase::on_get(const std::string & path, const HTTPS_HANDLER & handler) {
    return get_route_tree_.add_route(path, handler);
}

bool HttpsServerBase::on_post(const std::string & path, const HTTPS_HANDLER & handler) {
    return post_route_tree_.add_route(path, handler);
}

bool HttpsServerBase::on_websocket(const std::string & path, const WS_HANDLER & handler) {
    return websocket_route_tree_.add_route(path, handler);
}

bool HttpsServerBase::on_static_fs(const std::string & path, const std::string & root_path) {
    if (!boost::ends_with(path, "/*")) {
        spdlog::error("create static fs failed, path:{} is not end with *", path);
        return false;
    }

    std::string prefix = path.substr(0, path.size() - 1);
    return get_route_tree_.add_route(path, std::bind(&HttpsServerBase::static_fs_handler, this, root_path, prefix, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

boost::asio::awaitable<void> HttpsServerBase::static_fs_handler(std::string root_path, std::string prefix, std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp) {
    (void)session;
    spdlog::info("get static file request, path:{}", prefix + req->get_path_param("*"));
    std::ifstream file;
    auto suffix = req->get_path_param("*");
    file.open(root_path + prefix + suffix);
    if (!file.is_open()) {
        resp->add_header("Connection", "close");
        co_await resp->write_header(404, "Not Found");
        resp->close();
        co_return;
    }

    auto pos = suffix.find_last_of(".");
    if (pos == std::string::npos) {
        resp->add_header("Connection", "close");
        co_await resp->write_header(404, "Not Found");
        resp->close();
        co_return;
    }

    auto ext_name = suffix.substr(pos);
    auto it_content_type = http_content_type_map.find(ext_name);
    if (it_content_type == http_content_type_map.end()) {
        resp->add_header("Connection", "close");
        co_await resp->write_header(403, "Forbidden");
        resp->close();
        co_return;
    }
    
    resp->add_header("Content-Type", it_content_type->second);
    resp->add_header("Connection", "close");
    file.seekg(0, std::ios::end);
    int32_t length = file.tellg();
    file.seekg(0, std::ios::beg);
    std::unique_ptr<char[]> body = std::make_unique<char[]>(length);
    file.read(body.get(), length);
    resp->add_header("Content-Length", std::to_string(length));
    if (!(co_await resp->write_header(200, "OK"))) {
        resp->close();
        co_return;
    }

    bool ret = co_await resp->write_data((uint8_t*)body.get(), length);
    if (!ret) {
        resp->close();
        co_return;
    }
    resp->close();
    co_return;
}

boost::asio::awaitable<bool> HttpsServerBase::on_new_request(std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp) {
    switch(req->get_method()) {
        case GET : {
            if (is_websocket_req(req)) {
                auto handler = websocket_route_tree_.get_route(req->get_path(), req->path_params());
                if (!handler.has_value()) {
                    resp->add_header("Connection", "close");
                    co_await resp->write_header(404, "Not Found");
                    resp->close();
                } else {
                    // 返回websocket的http响应
                    resp->add_header("Upgrade", "websocket");
                    resp->add_header("Connection", "Upgrade");
                    std::string sec_websocket_accept;
                    auto k = req->get_header("Sec-WebSocket-Key") + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
                    auto sha1 = Utils::sha1(k);
                    std::string base64;
                    Utils::encode_base64(sha1, base64);
                    resp->add_header("Sec-WebSocket-Accept", base64);
                    auto protocol = req->get_header("Sec-WebSocket-Protocol");
                    if (!protocol.empty()) {
                        resp->add_header("Sec-WebSocket-Protocol", protocol);
                    }
                    
                    if (!co_await resp->write_header(101, "Switching Protocols")) {
                        resp->close();
                        co_return false;
                    }

                    std::shared_ptr<WsConn> ws_conn = std::make_shared<WsConn>(resp->get_conn());
                    co_await handler.value()(session, req, ws_conn);
                }
            } else {
                auto handler = get_route_tree_.get_route(req->get_path(), req->path_params());
                if (!handler.has_value()) {//404
                    resp->add_header("Connection", "close");
                    co_await resp->write_header(404, "Not Found");
                    resp->close();
                } else {
                    co_await handler.value()(session, req, resp);
                }
            }
            break;
        }
        default : {
            break;
        }
    }
    co_return true;
}

bool HttpsServerBase::is_websocket_req(std::shared_ptr<HttpRequest> req) {
    auto & host = req->get_header("Host");
    if (host.empty()) {
        return false;
    }

    auto & upgrade = req->get_header("Upgrade");
    if (upgrade != "websocket") {
        return false;
    }

    auto & connection = req->get_header("Connection");
    if (connection != "Upgrade") {
        return false;
    }

    auto & sec_websocket_key = req->get_header("Sec-WebSocket-Key");
    if (sec_websocket_key.empty()) {
        return false;
    }

    auto & sec_websocket_version = req->get_header("Sec-WebSocket-Version");
    if (sec_websocket_version.empty()) {
        return false;
    }

    try {
        auto isec_websocket_version = std::atoi(sec_websocket_version.c_str());
        if (isec_websocket_version != 13) {
            return false;
        }
    } catch (std::exception & e) {
        return false;
    }

    // auto & sec_websocket_protocol = req->get_header("Sec-WebSocket-Protocol");
    // if (sec_websocket_protocol.empty()) {
    //     return false;
    // }

    return true;
}
