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

// 应用自己的业务逻辑，回调，禁播，鉴权，统计，带宽，日志等
namespace mms {
class Session;
class StreamSession;
class AppConfig;
class OriginPullConfig;
class MediaSource;
class MediaSink;
class HttpCallbackConfig;

class App : public std::enable_shared_from_this<App> {
public:
    App(const std::string & domain, const std::string & app_name);
    virtual ~App();
    bool is_publish_app() const {
        return is_publish_app_;
    }
public:
    virtual void init();
    virtual void uninit();
    
    const std::string & get_domain_name() const {
        return domain_name_;
    }

    const std::string & get_app_name() const {
        return app_name_;
    }
    
    std::shared_ptr<AppConfig> get_conf();
    void update_conf(std::shared_ptr<AppConfig> config);

    spdlog::logger *get_logger();
protected:
    bool is_publish_app_ = false;
    std::string domain_name_;
    std::string app_name_;
    std::atomic_flag uninited_ = ATOMIC_FLAG_INIT;
    std::shared_ptr<AppConfig> app_conf_;
    spdlog::logger* app_logger_ptr_ = nullptr;
    std::shared_ptr<spdlog::logger> app_logger_; 
};
};