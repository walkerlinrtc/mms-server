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
class PublishApp;

class PlayApp : public App {
public:
    PlayApp(const std::string & domain, const std::string & app_name);
    virtual ~PlayApp();

    virtual boost::asio::awaitable<Error> on_play(std::shared_ptr<StreamSession> session);
    std::shared_ptr<PublishApp> get_publish_app();
    void set_publish_app(std::shared_ptr<PublishApp> publish_app);
private:
    std::shared_ptr<PublishApp> publish_app_;
    Error play_auth_check(std::shared_ptr<StreamSession> session);
};
};