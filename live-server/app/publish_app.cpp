#include "publish_app.h"
#include "config/app_config.h"
#include "config/cdn/origin_pull_config.h"
#include "config/cdn/edge_push_config.h"
#include "config/http_callback_config.h"

#include "protocol/http/http_request.hpp"
#include "protocol/http/http_response.hpp"

#include "json/json.h"
#include "sdk/http/http_client.hpp"

#include "client/http/http_flv_client_session.hpp"
#include "client/rtmp/rtmp_play_client_session.hpp"
#include "client/rtmp/rtmp_publish_client_session.hpp"

#include "server/session.hpp"
#include "core/stream_session.hpp"
#include "core/media_source.hpp"
#include "core/flv_media_source.hpp"
#include "core/rtmp_media_source.hpp"
#include "core/rtmp_media_sink.hpp"

#include "config/auth/auth_config.h"

#include "service/dns/dns_service.hpp"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "protocol/ts/ts_segment.hpp"


using namespace mms;
PublishApp::PublishApp(const std::string & domain_name, const std::string & app_name) : App(domain_name, app_name) {
    is_publish_app_ = true;
}

PublishApp::~PublishApp() {

}

boost::asio::awaitable<std::shared_ptr<MediaSource>> PublishApp::find_media_source(std::shared_ptr<StreamSession> session) {
    auto origin_pull_config = app_conf_->origin_pull_config();
    if (!origin_pull_config) {
        co_return nullptr;
    }

    if (origin_pull_config->get_protocol() == "http-flv") {
        auto url = origin_pull_config->gen_url(session);
        auto http_flv_client_session = std::make_shared<HttpFlvClientSession>(std::static_pointer_cast<PublishApp>(shared_from_this()), 
                                                                              session->get_worker(), 
                                                                              get_domain_name(), 
                                                                              get_app_name(), 
                                                                              session->get_stream_name());
        http_flv_client_session->set_pull_config(origin_pull_config);
        http_flv_client_session->set_url(url);
        http_flv_client_session->service();
        co_return http_flv_client_session->get_flv_media_source();
    } else if (origin_pull_config->get_protocol() == "rtmp") {
        auto url = origin_pull_config->gen_url(session);
        std::shared_ptr<RtmpPlayClientSession> rtmp_play_client_session = std::make_shared<RtmpPlayClientSession>(std::static_pointer_cast<PublishApp>(shared_from_this()),
                                                                                                                  session->get_worker(), 
                                                                                                                  get_domain_name(), 
                                                                                                                  get_app_name(), 
                                                                                                                  session->get_stream_name());
        rtmp_play_client_session->set_pull_config(origin_pull_config);
        rtmp_play_client_session->set_url(url);
        rtmp_play_client_session->service();
        co_return rtmp_play_client_session->get_rtmp_media_source();
    }
    co_return nullptr;
}

int32_t PublishApp::on_create_source(const std::string & domain, 
                                     const std::string & app_name,
                                     const std::string & stream_name, std::shared_ptr<MediaSource> source) {
    if (source->is_origin()) {//只有是源流（真实客户端推送的流，才算，还有种流，是通过中转拉取,不算源流）
        auto record_types = app_conf_->record_types();
        if (record_types.size() <= 0) {
            return 0;
        }

        for (auto & t : record_types) {
            CORE_INFO("session:{}/{}/{} try to create record:{}", domain, app_name, stream_name, t);
            auto recorder = source->get_or_create_recorder(t, std::static_pointer_cast<PublishApp>(shared_from_this()));
            if (recorder) {
                CORE_DEBUG("create recorder for:{}/{}/{} type:{} ok", domain, app_name, stream_name, t);
            }
        }
    }
    return 0;
}

void PublishApp::on_destroy_source(const std::string & domain, 
                                   const std::string & app_name,
                                   const std::string & stream_name, 
                                   std::shared_ptr<MediaSource> source) {
    (void)domain;
    (void)app_name;
    (void)stream_name;
    (void)source;
}

std::vector<std::shared_ptr<MediaSink>> PublishApp::create_push_streams(std::shared_ptr<MediaSource> source, std::shared_ptr<StreamSession> session) {
    std::vector<std::shared_ptr<MediaSink>> sinks;
    auto push_configs = app_conf_->edge_push_configs();
    for (auto & config : push_configs) {
        if (config->get_protocol() == "rtmp" && source->get_media_type() == "rtmp") {
            auto url = config->gen_url(session);
            std::shared_ptr<RtmpMediaSource> rtmp_source = std::static_pointer_cast<RtmpMediaSource>(source);
            auto rtmp_publish_client_session = std::make_shared<RtmpPublishClientSession>(
                                                            std::weak_ptr<RtmpMediaSource>(rtmp_source), 
                                                            std::static_pointer_cast<PublishApp>(shared_from_this()),
                                                            session->get_worker());
            rtmp_publish_client_session->set_url(url);
            rtmp_publish_client_session->set_push_config(config);
            auto sink = rtmp_publish_client_session->get_rtmp_media_sink();
            sinks.push_back(sink);
            rtmp_publish_client_session->service();
        }
    }
    return sinks;
}

boost::asio::awaitable<Error> PublishApp::on_publish(std::shared_ptr<StreamSession> session) {
    auto err = publish_auth_check(session);
    if (ERROR_SUCCESS != err.code) {
        co_return err;
    }

    auto on_publish_callbacks_config = app_conf_->get_on_publish_config();
    for (auto & config : on_publish_callbacks_config) {
        auto err = co_await invoke_on_publish_http_callback(config, session);
        if (0 != err.code) {
            co_return err;
        }
    }
    co_return Error{0, ""};
}

Error PublishApp::publish_auth_check(std::shared_ptr<StreamSession> session) {
    auto publish_auth_check = app_conf_->get_publish_auth_config();
    if (!publish_auth_check) {
        return Error{ERROR_SUCCESS, ""};
    }

    if (!publish_auth_check->is_enabled()) {
        return Error{ERROR_SUCCESS, ""};
    }

    if (0 != publish_auth_check->check(session)) {
        return Error{ERROR_FORBIDDEN, "Forbidden"};
    }
    return Error{ERROR_SUCCESS, ""};
}

boost::asio::awaitable<Error> PublishApp::invoke_on_publish_http_callback(const HttpCallbackConfig & conf, std::shared_ptr<StreamSession> session) {
    auto url = conf.gen_url(session);
    std::string protocol;
    std::string domain;
    uint16_t port;
    std::string path;

    std::unordered_map<std::string, std::string> params;
    if (!Utils::parse_url(url, protocol, domain, port, path, params)) {
        CORE_ERROR("parse on_publish notify url:{} failed", url);
        co_return Error{ERROR_SERVER_ERROR, "parse notify url failed"};
    }
    //获取域名ip
    auto ip = DnsService::get_instance().get_ip(conf.get_domain_name());
    if (!ip) {
        co_return Error{ERROR_SERVER_ERROR, "could not find ip"};
    }
    auto & server_ip = ip.value();
    
    // 填写请求参数
    HttpRequest http_req;
    http_req.set_method(POST);
    http_req.set_path(path);
    http_req.set_query_params(params);
    http_req.set_version("1.1");

    std::string body = conf.gen_body(session);
    auto headers = conf.gen_headers(session);
    for (auto & h : headers) {
        http_req.add_header(h.first, h.second);
    }

    http_req.add_header("Content-Length", std::to_string(body.size()));
    http_req.set_body(body);

    std::shared_ptr<HttpClient> http_client;
    if (protocol == "http") {
        http_client = std::make_shared<HttpClient>(session->get_worker(), false);
    } else if (protocol == "https") {
        http_client = std::make_shared<HttpClient>(session->get_worker(), true);
    } else {
        co_return  Error{ERROR_SERVER_ERROR, "unsupported protocol"};
    }

    auto resp = co_await http_client->do_req(server_ip, conf.get_port(), http_req);
    if (!resp) {
        co_return Error{ERROR_SERVER_ERROR, "http client connect failed"};
    }

    bool ret = co_await resp->recv_http_header();
    if (!ret) {
        co_return Error{ERROR_SERVER_ERROR, "http client connect failed"};
    }

    if (resp->get_status_code() != 200) {
        co_return Error{resp->get_status_code(), resp->get_header("reason")};
    }

    int32_t code;
    std::string msg;
    // todo : 这里session已经在函数hold住了，lambda没必要传值，但是传值会导致内存异常，不知道为什么，还需要研究下
    co_await resp->cycle_recv_body([this, &session, &code, &msg](const std::string_view & recv_data)->boost::asio::awaitable<int32_t> {
        JSONCPP_STRING errs;
        Json::CharReaderBuilder readerBuilder;
        std::unique_ptr<Json::CharReader> const jsonReader(readerBuilder.newCharReader());
        Json::Value root;
        if (!jsonReader->parse((char*)recv_data.data(), (char*)recv_data.data() + recv_data.size(), &root, &errs)) {
            co_return 0;
        }

        if (!errs.empty()) {
            co_return 0;
        }

        if (!root.isMember("code") || !root["code"].isInt()) {
            co_return recv_data.size();
        }

        code = root["code"].asInt();
        if (code != 0) {
            co_return recv_data.size();
        }

        if (root.isMember("msg") && root["msg"].isString()) {
            msg = root["msg"].asString();
        }

        co_return recv_data.size();
    });

    http_client->close();
    co_return Error{code, msg};
}

boost::asio::awaitable<Error> PublishApp::on_unpublish(std::shared_ptr<StreamSession> session) {
    auto on_unpublish_callbacks_config = app_conf_->get_on_unpublish_config();
    for (auto & config : on_unpublish_callbacks_config) {
        auto err = co_await invoke_on_unpublish_http_callback(config, session);
        if (0 != err.code) {
            co_return err;
        }
    }
    co_return Error{0, ""};
}

boost::asio::awaitable<Error> PublishApp::invoke_on_unpublish_http_callback(const HttpCallbackConfig & conf, std::shared_ptr<StreamSession> session) {
    auto url = conf.gen_url(session);
    std::string protocol;
    std::string domain;
    uint16_t port;
    std::string path;
    std::unordered_map<std::string, std::string> params;
    if (!Utils::parse_url(url, protocol, domain, port, path, params)) {
        CORE_ERROR("parse on_publish notify url:{} failed", url);
        co_return Error{ERROR_SERVER_ERROR, "parse notify url failed"};
    }
    //获取域名ip
    auto ip = DnsService::get_instance().get_ip(conf.get_domain_name());
    if (!ip) {
        co_return Error{ERROR_SERVER_ERROR, "could not find ip"};
    }
    auto & server_ip = ip.value();
    // 填写请求参数
    HttpRequest http_req;
    http_req.set_method(POST);
    http_req.set_path(path);
    http_req.set_query_params(params);
    http_req.set_version("1.1");

    Json::Value root;
    root["event"] = "on_unpublish";
    root["stream_type"] = session->get_session_type();
    root["domain"] = session->get_domain_name();
    root["app_name"] = session->get_app_name();
    root["stream_name"] = session->get_stream_name();
    std::string body = root.toStyledString();
    http_req.add_header("Host", session->get_domain_name());
    http_req.add_header("Content-Type", "application/json");
    http_req.add_header("Content-Length", std::to_string(body.size()));
    http_req.set_body(body);

    std::shared_ptr<HttpClient> http_client;
    if (protocol == "http") {
        http_client = std::make_shared<HttpClient>(session->get_worker(), false);
    } else if (protocol == "https") {
        http_client = std::make_shared<HttpClient>(session->get_worker(), true);
    } else {
        co_return Error{ERROR_SERVER_ERROR, "unsupported protocol"};
    }

    auto resp = co_await http_client->do_req(server_ip, conf.get_port(), http_req);
    bool ret = co_await resp->recv_http_header();
    if (!ret) {
        co_return Error{ERROR_SERVER_ERROR, "recv http header failed"};
    }

    if (resp->get_status_code() != 200) {
        co_return Error{resp->get_status_code(), resp->get_header("reason")};
    }

    int32_t code;
    std::string msg;
    // todo : 这里session已经在函数hold住了，lambda没必要传值，但是传值会导致内存异常
    co_await resp->cycle_recv_body([this, &session, &code, &msg](const std::string_view & recv_data)->boost::asio::awaitable<int32_t> {
        JSONCPP_STRING errs;
        Json::CharReaderBuilder readerBuilder;
        std::unique_ptr<Json::CharReader> const jsonReader(readerBuilder.newCharReader());
        Json::Value root;
        if (!jsonReader->parse((char*)recv_data.data(), (char*)recv_data.data() + recv_data.size(), &root, &errs)) {
            co_return 0;
        }

        if (!errs.empty()) {
            co_return 0;
        }

        if (!root.isMember("code") || !root["code"].isInt()) {
            co_return recv_data.size();
        }

        code = root["code"].asInt();
        if (code != 0) {
            co_return recv_data.size();
        }

        if (root.isMember("msg") && root["msg"].isString()) {
            msg = root["msg"].asString();
        }

        co_return recv_data.size();
    });
    http_client->close();
    co_return Error{code, msg};
}

bool PublishApp::can_reap_ts(bool is_key, std::shared_ptr<TsSegment> ts_seg) {
    if (!ts_seg->has_video()) {
        return ts_seg->get_duration() >= app_conf_->hls_config().ts_min_seg_dur();
    }

    if ((ts_seg->get_duration() >= app_conf_->hls_config().ts_min_seg_dur() && is_key)|| 
        ts_seg->get_duration() >= app_conf_->hls_config().ts_max_seg_dur()) {
        return true;
    }

    if (ts_seg->get_ts_bytes() >= app_conf_->hls_config().ts_max_size()) {
        return true;
    }

    return false;
}

bool PublishApp::can_reap_mp4(bool is_key, int64_t duration, int64_t bytes) {
    if ((duration >= app_conf_->get_fmp4_min_seg_dur() && is_key)|| 
        duration >= app_conf_->get_fmp4_max_seg_dur()) {
        return true;
    }

    if (bytes >= app_conf_->get_max_fmp4_seg_bytes()) {
        return true;
    }

    return false;
}