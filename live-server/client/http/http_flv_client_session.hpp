/*
 * @Author: jbl19860422
 * @Date: 2023-12-24 14:04:07
 * @LastEditTime: 2023-12-27 13:55:27
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\client\http\httpflv_client_session.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <atomic>
#include <string>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>

#include "core/stream_session.hpp"
#include "base/sequence_pkt_buf.hpp"
#include "base/wait_group.h"
#include "base/network/tcp_socket.hpp"

namespace mms {
class App;
class PublishApp;
class ThreadWorker;
class FlvMediaSource;
class HttpResponse;
class HttpClient;
class FlvTag;
class OriginPullConfig;

class HttpFlvClientSession : public StreamSession {
public:
    HttpFlvClientSession(std::shared_ptr<PublishApp> app, ThreadWorker *worker, 
                         const std::string & org_domain, const std::string & org_app_name , const std::string & org_stream_name);
    virtual ~HttpFlvClientSession();
    void set_url(const std::string & url);
    std::shared_ptr<FlvMediaSource> get_flv_media_source();
    void start();
    void stop();
    void set_pull_config(std::shared_ptr<OriginPullConfig> pull_config);
private:
    boost::asio::awaitable<void> cycle_pull_flv_tag(std::shared_ptr<HttpResponse> resp);
private:
    std::shared_ptr<OriginPullConfig> pull_config_;

    std::shared_ptr<FlvMediaSource> flv_media_source_;
    std::shared_ptr<HttpClient> http_client_;

    SequencePktBuf<FlvTag> flv_tags_;
    std::shared_ptr<FlvTag> cache_tag_;
    uint32_t prev_tag_size_ = 0;
    std::string url_;
    boost::asio::steady_timer check_closable_timer_;
    WaitGroup wg_;
};
};