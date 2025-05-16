/*
 * @Author: jbl19860422
 * @Date: 2023-09-16 10:32:17
 * @LastEditTime: 2023-12-27 21:23:40
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\http\http_server_session.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "http_server_session.hpp"
#include "base/network/socket_interface.hpp"

using namespace mms;

HttpServerSession::HttpServerSession(HttpRequestHandler *request_handler, std::shared_ptr<SocketInterface> sock):
                    Session(sock->get_worker()), request_handler_(request_handler), sock_(sock), wg_(worker_) {
    set_session_type("http");
    buf_ = std::make_unique<char[]>(HTTP_MAX_BUF);
}

HttpServerSession::~HttpServerSession() {
    // CORE_DEBUG("destroy HttpServerSession");
}

std::shared_ptr<SocketInterface> HttpServerSession::get_sock() {
    return sock_;
}

void HttpServerSession::service() {
    auto self(this->shared_from_this());
    // todo:consider to wrap the conn as a bufio, and move parser to HttpRequest class.
    wg_.add(1);
    boost::asio::co_spawn(sock_->get_worker()->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        http_parser_.on_http_request(std::bind(&HttpServerSession::on_http_request, this, std::placeholders::_1));
        co_await cycle_recv();
        co_return;
    }, [this, self](std::exception_ptr exp) {
        (void)exp;
        wg_.done();
        close();
    });
}

void HttpServerSession::close() {
    if (closed_.test_and_set()) {
        return;
    }

    auto self(this->shared_from_this());
    boost::asio::co_spawn(get_worker()->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        boost::system::error_code ec;
        if (sock_) {
            sock_->close();
        }

        co_await wg_.wait();
        sock_.reset();
        co_return;
    }, boost::asio::detached);
}

boost::asio::awaitable<void> HttpServerSession::cycle_recv() {
    bool cont;
    int32_t consumed;
    if (buf_size_ > 0) {
        do {
            std::tie(cont, consumed) = co_await parse_recv_buf((const char*)buf_.get(), buf_size_);
            if (consumed < 0) {
                close();
                co_return;
            }

            if (consumed > 0) {
                buf_size_ -= consumed;
                memmove((void*)buf_.get(), (void*)(buf_.get() + consumed), buf_size_);
            }

            if (!cont) {
                break;
            }
        } while (consumed > 0 && cont);
    }

    while(1) {
        auto recv_size = co_await sock_->recv_some((uint8_t*)buf_.get() + buf_size_, HTTP_MAX_BUF - buf_size_);
        if (recv_size < 0) {
            break;
        }

        buf_size_ += recv_size;   
        std::tie(cont, consumed) = co_await parse_recv_buf((const char*)buf_.get(), buf_size_);
        if (consumed < 0) {
            close();
            co_return;
        }

        if (consumed > 0) {
            buf_size_ -= consumed;
            memmove((void*)buf_.get(), (void*)(buf_.get() + consumed), buf_size_);
        }

        if (!cont) {
            break;
        }
    }

    co_return;
}

boost::asio::awaitable<std::pair<bool,int32_t>> HttpServerSession::parse_recv_buf(const char *buf, int32_t len) {
    int32_t total_consumed = 0;
    int32_t consumed = 0;
    do {
        consumed = co_await http_parser_.read(std::string_view(buf + total_consumed, len));
        if (consumed < 0) {
            co_return std::make_pair(false, -1);
        }
        total_consumed += consumed;
        len -= consumed;
        if (is_websocket_) {
            co_return std::make_pair(false, total_consumed);
        }
    } while(consumed > 0 && len > 0);

    co_return std::make_pair(true, total_consumed);
}

boost::asio::awaitable<void> HttpServerSession::on_http_request(std::shared_ptr<HttpRequest> req) {
    if (curr_req_) {
        reqs_.push_back(req);
        co_return;
    }
    
    curr_req_ = req;
    co_await process_one_req(curr_req_);
    co_return;
}

boost::asio::awaitable<void> HttpServerSession::process_one_req(std::shared_ptr<HttpRequest> req) {
    auto self(shared_from_this());
    if (is_websocket_req(req)) {
        is_websocket_ = true;//升级为websocket处理
    }

    boost::asio::co_spawn(get_worker()->get_io_context(), [this, self, req]()->boost::asio::awaitable<void> {
        std::shared_ptr<HttpResponse> resp = std::make_shared<HttpResponse>(sock_);
        co_await request_handler_->on_new_request(std::static_pointer_cast<HttpServerSession>(shared_from_this()), req, resp); 
        co_return;
    }, boost::asio::detached);
    co_return;
}

boost::asio::awaitable<void> HttpServerSession::close_or_do_next_req() {
    if (!curr_req_) {
        co_return;
    }
    
    if (curr_req_->get_header("Connection") != "keep-alive") {// 非长连接，关闭
        sock_->close();
        co_return;
    }

    // 必然在最前面，去掉最前面的请求
    if (reqs_.size() <= 0) {
        curr_req_ = nullptr;
        co_return;
    }

    curr_req_ = reqs_.front();
    reqs_.pop_front();
    
    co_await process_one_req(curr_req_);
    co_return;
}

bool HttpServerSession::is_websocket_req(std::shared_ptr<HttpRequest> req) {
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