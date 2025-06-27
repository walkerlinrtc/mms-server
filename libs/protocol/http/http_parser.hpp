/*
 * @Author: jbl19860422
 * @Date: 2023-12-18 23:50:29
 * @LastEditTime: 2023-12-27 14:00:02
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\protocol\http\http_parser.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <memory>
#include <functional>
#include <boost/asio/awaitable.hpp>

#include "http_define.h"
#include "http_request.hpp"

namespace mms {
enum HTTP_STATE {
    HTTP_STATE_WAIT_REQUEST_LINE    =   0,
    HTTP_STATE_WAIT_HEADER          =   1,
    HTTP_STATE_REQUEST_BODY         =   2,
    HTTP_STATE_REQUEST_ERROR        =   3,
};

class HttpParser {
public:
    boost::asio::awaitable<int32_t> read(const std::string_view & buf);
    void on_http_request(const std::function<void(std::shared_ptr<HttpRequest>)> & cb);
    virtual ~HttpParser();
private:
    std::shared_ptr<HttpRequest> http_req_;
    std::function<void(std::shared_ptr<HttpRequest>)> req_cb_;
    HTTP_STATE state_ = HTTP_STATE_WAIT_REQUEST_LINE;
};

};