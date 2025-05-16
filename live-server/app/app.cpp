#include "spdlog/spdlog.h"
#include "app.h"
#include "config/app_config.h"
#include "config/cdn/origin_pull_config.h"
#include "config/cdn/edge_push_config.h"
#include "config/http_callback_config.h"

#include "protocol/http/http_request.hpp"
#include "protocol/http/http_response.hpp"

#include "json/json.h"
#include "sdk/http/http_client.hpp"
// #include "client/http/httpflv_client_session.hpp"
// #include "server/rtmp/rtmp_play_client_session.hpp"

#include "server/session.hpp"
#include "core/stream_session.hpp"
// #include "core/media_source.hpp"
// #include "core/flv_media_source.hpp"

#include "config/auth/auth_config.h"
// #include "server/rtmp/rtmp_publish_client_session.hpp"

#include "service/dns/dns_service.hpp"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"

using namespace mms;
App::App(const std::string & domain, const std::string & app_name):domain_name_(domain), app_name_(app_name) {

}

App::~App() {
    uninit();
}

void App::init() {
    try {
        app_logger_ = spdlog::daily_logger_mt(domain_name_ + "/" + app_name_, "logs/" + domain_name_ + "/" + app_name_ + ".log", 00, 00);
        app_logger_ptr_ = app_logger_.get();
        app_logger_ptr_->flush_on(spdlog::level::info);
    } catch (const spdlog::spdlog_ex & ex) {
        spdlog::error("create {}/{}'s log failed", domain_name_, app_name_);
    }
}

void App::uninit() {
    if (uninited_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    if (app_logger_) {
        app_logger_->flush();
        app_logger_.reset();
    }
}

spdlog::logger *App::get_logger() {
    return app_logger_ptr_;
}

void App::update_conf(std::shared_ptr<AppConfig> app_config) {
    app_conf_ = app_config;
}

std::shared_ptr<AppConfig> App::get_conf() {
    return app_conf_;
}