/*
 * @Author: jbl19860422
 * @Date: 2023-09-24 09:08:29
 * @LastEditTime: 2023-10-01 21:09:07
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\http\http_server_base.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <memory>
#include <functional>
#include <boost/asio/awaitable.hpp>

#include "server/tcp/tcp_server.hpp"
#include "base/thread/thread_pool.hpp"
#include "protocol/http/http_define.h"
#include "http_server_session.hpp"
#include "router/trie_tree.hpp"
namespace mms {
class WsConn;
class HttpRequest;
class HttpResponse;

class HttpServerBase : public TcpServer, public SocketInterfaceHandler, public HttpRequestHandler {
    using HTTP_HANDLER = std::function<boost::asio::awaitable<void>(std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp)>;
    using WS_HANDLER = std::function<boost::asio::awaitable<void>(std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<WsConn> ws_conn)>;
public:
    HttpServerBase(ThreadWorker *w);
    virtual ~HttpServerBase();
    bool start(uint16_t port = 80, const std::string & ip = "");
    void stop();
protected:
    bool on_get(const std::string & path, const HTTP_HANDLER & handler);
    bool on_options(const std::string & path, const HTTP_HANDLER & handler);
    bool on_post(const std::string & path, const HTTP_HANDLER & handler);
    bool on_patch(const std::string & path, const HTTP_HANDLER & handler);
    bool on_delete(const std::string & path, const HTTP_HANDLER & handler);
    bool on_websocket(const std::string & path, const WS_HANDLER & handler);
    bool on_static_fs(const std::string & path, const std::string & root_path);
    virtual bool register_route();
    // TcpServer interface
    void on_socket_open(std::shared_ptr<SocketInterface> tcp_socket) override;
    void on_socket_close(std::shared_ptr<SocketInterface> tcp_socket) override;
    // HttpServerSession interface
    boost::asio::awaitable<bool> on_new_request(std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp) override;
    // judge if websocket request
    bool is_websocket_req(std::shared_ptr<HttpRequest> req);
    //
    boost::asio::awaitable<void> static_fs_handler(std::string root_path, std::string prefix, std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp);
protected:
    TrieTree<HTTP_HANDLER> get_route_tree_;
    TrieTree<HTTP_HANDLER> post_route_tree_;
    TrieTree<HTTP_HANDLER> put_route_tree_;
    TrieTree<HTTP_HANDLER> head_route_tree_;
    TrieTree<HTTP_HANDLER> delete_route_tree_;
    TrieTree<HTTP_HANDLER> options_route_tree_;
    TrieTree<HTTP_HANDLER> patch_route_tree_;
    // websocket路由
    TrieTree<WS_HANDLER> websocket_route_tree_;
};
};