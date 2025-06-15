#include <boost/asio/awaitable.hpp>
#include "http_parser.hpp"
#include "spdlog/spdlog.h"

using namespace mms;

boost::asio::awaitable<int32_t> HttpParser::read(const std::string_view & buf) {
    switch(state_) {
        case HTTP_STATE_WAIT_REQUEST_LINE: {
            http_req_ = std::make_shared<HttpRequest>();
            auto pos = buf.find("\r\n");
            if (pos != std::string::npos) {
                if (!http_req_->parse_request_line(buf.data(), pos)) {
                    state_ = HTTP_STATE_REQUEST_ERROR;
                    co_return -1;
                } else {
                    state_ = HTTP_STATE_WAIT_HEADER;                        
                    co_return pos + 2;
                }
            } else {
                co_return 0;
            }
            break;
        }
        case HTTP_STATE_WAIT_HEADER: {
            auto pos = buf.find("\r\n");
            if (pos != std::string::npos) {
                if (pos == 0) {
                    // content-len 得到长度
                    if (http_req_->get_method() == GET) {
                        state_ = HTTP_STATE_WAIT_REQUEST_LINE;
                        co_await req_cb_(http_req_);
                        co_return pos + 2;
                    } else {
                        if (http_req_->get_header("Content-Length") == "") {
                            co_await req_cb_(http_req_);
                            co_return pos + 2;
                        } 

                        state_ = HTTP_STATE_REQUEST_BODY;
                        co_return pos + 2;
                    }
                } else {
                    if (!http_req_->parse_header(buf.data(), pos)) {
                        state_ = HTTP_STATE_REQUEST_ERROR;
                        co_return -2;
                    } else {
                        state_ = HTTP_STATE_WAIT_HEADER;
                        co_return pos + 2;
                    }
                }
            }
            break;
        }
        case HTTP_STATE_REQUEST_BODY: {
            try {
                //todo 这里要考虑多些
                auto consumed = http_req_->parse_body(buf.data(), buf.size());
                if (consumed < 0) {
                    state_ = HTTP_STATE_REQUEST_ERROR;
                    co_return -5;
                }
                co_await req_cb_(std::move(http_req_));
                state_ = HTTP_STATE_WAIT_REQUEST_LINE;
                co_return consumed;
            } catch(std::exception & e) {
                co_return -4;
            }
            break;
        }
        default: {
            co_return 0;
        }
    }
    co_return 0;
}

void HttpParser::on_http_request(const std::function<boost::asio::awaitable<void>(std::shared_ptr<HttpRequest>)> & cb) {
    req_cb_ = cb;
}

HttpParser::~HttpParser() {
}