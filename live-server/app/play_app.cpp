#include "play_app.h"
#include "config/app_config.h"
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
PlayApp::PlayApp(const std::string & domain_name, const std::string & app_name) : App(domain_name, app_name) {
    is_publish_app_ = false;
}

PlayApp::~PlayApp() {

}

std::shared_ptr<PublishApp> PlayApp::get_publish_app() {
    return publish_app_;
}

void PlayApp::set_publish_app(std::shared_ptr<PublishApp> publish_app) {
    publish_app_ = publish_app;
}

boost::asio::awaitable<Error> PlayApp::on_play(std::shared_ptr<StreamSession> session) {
    auto err = play_auth_check(session);
    if (ERROR_SUCCESS != err.code) {
        co_return err;
    }

    co_return Error{0, ""};
}

Error PlayApp::play_auth_check(std::shared_ptr<StreamSession> session) {
    auto play_auth_check = app_conf_->get_play_auth_config();
    if (!play_auth_check) {
        return Error{ERROR_SUCCESS, ""};
    }

    if (!play_auth_check->is_enabled()) {
        return Error{ERROR_SUCCESS, ""};
    }

    if (0 != play_auth_check->check(session)) {
        return Error{ERROR_FORBIDDEN, "Forbidden"};
    }
    
    return Error{ERROR_SUCCESS, ""};
}