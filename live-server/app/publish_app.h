/*
 * @Author: jbl19860422
 * @Date: 2023-05-29 07:19:31
 * @LastEditTime: 2023-12-23 16:16:16
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\app\app.h
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <string>
#include <memory>
#include <atomic>
#include <boost/asio/awaitable.hpp>

#include "core/media_source_finder.h"
#include "core/error_code.hpp"
#include "log/log.h"
#include "app.h"

// 应用自己的业务逻辑，回调，禁播，鉴权，统计，带宽，日志等
namespace mms {
class Session;
class StreamSession;
class AppConfig;
class OriginPullConfig;
class MediaSource;
class MediaSink;
class HttpCallbackConfig;
class TsSegment;
class Mp4Segment;

class PublishApp : public App, public MediaSourceFinder {
public:
    PublishApp(const std::string & domain_name, const std::string & app_name);
    virtual ~PublishApp();
public:
    int32_t on_create_source(const std::string & domain, 
                             const std::string & app_name,
                             const std::string & stream_name, 
                             std::shared_ptr<MediaSource> source);
    boost::asio::awaitable<std::shared_ptr<MediaSource>> find_media_source(std::shared_ptr<StreamSession> session) override;
    std::vector<std::shared_ptr<MediaSink>> create_push_streams(std::shared_ptr<MediaSource> source, std::shared_ptr<StreamSession> session);

    virtual boost::asio::awaitable<Error> on_publish(std::shared_ptr<StreamSession> session);
    virtual boost::asio::awaitable<Error> on_unpublish(std::shared_ptr<StreamSession> session);
    virtual bool can_reap_ts(bool is_key, std::shared_ptr<TsSegment> ts_seg);
    virtual bool can_reap_mp4(bool is_key, int64_t duration, int64_t seg_bytes);
private:
    Error publish_auth_check(std::shared_ptr<StreamSession> session);
    boost::asio::awaitable<Error> invoke_on_publish_http_callback(const HttpCallbackConfig & conf, std::shared_ptr<StreamSession> session);
    boost::asio::awaitable<Error> invoke_on_unpublish_http_callback(const HttpCallbackConfig & conf, std::shared_ptr<StreamSession> session);
};
};