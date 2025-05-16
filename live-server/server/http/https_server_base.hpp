/*
 * @Author: jbl19860422
 * @Date: 2023-09-24 16:13:28
 * @LastEditTime: 2023-12-27 14:17:39
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\http\https_server_base.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <memory>

#include "server/tls/tls_server.hpp"
#include "protocol/http/http_define.h"
#include "http_server_session.hpp"
#include "router/trie_tree.hpp"

namespace mms {
class WsConn;
class HttpsServerBase : public HttpRequestHandler, public SocketInterfaceHandler, public TlsServerNameHandler {
    using HTTPS_HANDLER = std::function<boost::asio::awaitable<void>(std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)>;
    using WS_HANDLER = std::function<boost::asio::awaitable<void>(std::shared_ptr<HttpServerSession> session,std::shared_ptr<HttpRequest> req, std::shared_ptr<WsConn> ws_conn)>;
public:
    HttpsServerBase(ThreadWorker *w);
    virtual ~HttpsServerBase();
    
    bool start(uint16_t port = 443);
    void stop();
    bool on_get(const std::string & path, const HTTPS_HANDLER & handler);
    bool on_post(const std::string & path, const HTTPS_HANDLER & handler);
    bool on_websocket(const std::string & path, const WS_HANDLER & handler);
    bool on_static_fs(const std::string & path, const std::string & root_path);
protected:
    virtual bool register_route();
protected:
    void on_socket_open(std::shared_ptr<SocketInterface> socket) override;
    void on_socket_close(std::shared_ptr<SocketInterface> socket) override;
    boost::asio::awaitable<bool> on_new_request(std::shared_ptr<HttpServerSession> session,std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp) override;
    // judge if websocket request
    bool is_websocket_req(std::shared_ptr<HttpRequest> req);
    boost::asio::awaitable<void> static_fs_handler(std::string root_path, std::string prefix, std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp);
protected:
    std::unique_ptr<TlsServer> tls_server_;
    TrieTree<HTTPS_HANDLER> get_route_tree_;
    TrieTree<HTTPS_HANDLER> post_route_tree_;
    TrieTree<HTTPS_HANDLER> put_route_tree_;
    TrieTree<HTTPS_HANDLER> head_route_tree_;
    TrieTree<HTTPS_HANDLER> delete_route_tree_;
    TrieTree<HTTPS_HANDLER> options_route_tree_;
    // websocket路由
    TrieTree<WS_HANDLER> websocket_route_tree_;
};
};