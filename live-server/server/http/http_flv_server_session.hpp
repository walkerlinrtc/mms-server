/*
 * @Author: jbl19860422
 * @Date: 2023-12-22 22:33:05
 * @LastEditTime: 2023-12-27 14:55:57
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\http\http_flv_server_session.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <memory>
#include <atomic>
#include <vector>
#include <fstream>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/buffer.hpp>

#include "server/session.hpp"
#include "core/stream_session.hpp"
#include "base/utils/utils.h"
#include "base/wait_group.h"

namespace mms {
class HttpRequest;
class HttpResponse;
class FlvMediaSink;
class FlvTag;
class MediaEvent;

class HttpFlvServerSession : public StreamSession {
public:
    HttpFlvServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp);
    virtual ~HttpFlvServerSession();
    void service();
    void close(bool close_connection);
    void close();
private:
    boost::asio::awaitable<bool> send_flv_tags(std::vector<std::shared_ptr<FlvTag>> tags);
    void process_media_event(const MediaEvent & ev);

    void start_send_coroutine();
    void start_alive_checker();
    void update_active_timestamp();
private:
    std::shared_ptr<FlvMediaSink> flv_media_sink_;

    std::shared_ptr<HttpRequest> http_request_;
    std::shared_ptr<HttpResponse> http_response_;

    boost::asio::experimental::channel<void(boost::system::error_code, std::function<boost::asio::awaitable<bool>()>)> send_funcs_channel_;
    std::vector<boost::asio::const_buffer> send_bufs_;
    uint32_t prev_tag_size_ = 0;

    int64_t last_active_time_ = Utils::get_current_ms();
    boost::asio::steady_timer alive_timeout_timer_;//超时定时器，如果超过一段时间，没有任何数据，则关闭session 

    int32_t prev_tag_sizes_[1024];

    WaitGroup wg_;
    bool has_write_flv_header_ = false;
};

};