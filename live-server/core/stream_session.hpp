#pragma once
#include <string>
#include <optional>
#include <unordered_map>
#include <memory>
#include <boost/asio/steady_timer.hpp>

#include "server/session.hpp"
#include "base/wait_group.h"

namespace mms {
class AppConfig;
class App;
class MediaSource;
class BitrateMonitor;

class StreamSession : public Session {
public:
    StreamSession(ThreadWorker *worker);
    virtual ~StreamSession();
public:
    void set_session_info(const std::string & domain, const std::string & app_name, const std::string & stream_name);
    void set_session_info(const std::string_view & domain, const std::string_view & app_name, const std::string_view & stream_name);

    const std::string & get_domain_name() const {
        return domain_name_;
    }

    const std::string & get_app_name() const {
        return app_name_;
    }

    const std::string & get_stream_name() const {
        return stream_name_;
    }

    void set_app_conf(std::shared_ptr<AppConfig> app_conf) {
        app_conf_ = app_conf;
    }

    std::shared_ptr<AppConfig> get_app_conf() {
        return app_conf_;
    }

    bool find_and_set_app(const std::string & domain_name, const std::string & app_name, bool is_publish);
    void set_app(std::shared_ptr<App> app) {
        app_ = app;
    }
    
    std::shared_ptr<App> get_app() {
        return app_;
    }

    std::string get_client_ip();
public:
    void start_delayed_source_check_and_delete(uint32_t delay_sec, std::shared_ptr<MediaSource> source);//启动source延迟关闭
protected:
    bool is_publish_ = false;
    std::string domain_name_;
    std::string app_name_;
    std::string stream_name_;
    std::shared_ptr<AppConfig> app_conf_;
    std::shared_ptr<App> app_;
    WaitGroup wg_;
    std::atomic<bool> waiting_cleanup_{false};
    boost::asio::steady_timer cleanup_timer_;

    std::unique_ptr<BitrateMonitor> video_bitrate_monitor_;
    std::unique_ptr<BitrateMonitor> audio_bitrate_monitor_;
};
};