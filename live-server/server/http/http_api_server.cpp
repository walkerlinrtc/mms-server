#include "log/log.h"

#include <boost/shared_ptr.hpp>
#include <memory>
#include <string>

#include "http_api_server.hpp"
#include "protocol/http/http_server_session.hpp"
#include "http_flv_server_session.hpp"
#include "http_ts_server_session.hpp"
#include "http_m3u8_server_session.hpp"
#include "http_mpd_server_session.hpp"
#include "http_m4s_server_session.hpp"

#include "http_long_m4s_server_session.hpp"
#include "http_long_ts_server_session.hpp"

#include "core/stream_session.hpp"
#include "core/source_manager.hpp"
#include "core/media_source.hpp"

#include "config/config.h"
#include "config/app_config.h"
#include "app/app_manager.h"
#include "app/app.h"
#include "../../version.h"
#include "obj_viewer.h"
#include "recorder/recorder_manager.h"
#include "recorder/recorder.h"

using namespace mms;
bool HttpApiServer::register_route() {
    bool ret;
    ret = on_get("/api/version", std::bind(&HttpApiServer::get_api_version, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
    if (!ret) {
        return false;
    }

    ret = on_get("/api/obj_count", std::bind(&HttpApiServer::get_obj_count, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
    if (!ret) {
        return false;
    }

    ret = on_get("/api/domain_apps", std::bind(&HttpApiServer::get_domain_apps, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
    if (!ret) {
        return false;
    }

    ret = on_get("/api/domain_streams", std::bind(&HttpApiServer::get_domain_streams, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
    if (!ret) {
        return false;
    }

    ret = on_get("/api/app_streams", std::bind(&HttpApiServer::get_app_streams, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
    if (!ret) {
        return false;
    }

    ret = on_get("/api/domain_recorders", std::bind(&HttpApiServer::get_domain_recorders, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
    if (!ret) {
        return false;
    }

    ret = on_get("/api/app_recorders", std::bind(&HttpApiServer::get_app_recorders, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
    if (!ret) {
        return false;
    }

    ret = on_post("/api/stop_recorder", std::bind(&HttpApiServer::stop_recorder, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
    if (!ret) {
        return false;
    }

    ret = on_options("/api/stop_recorder", [this](std::shared_ptr<HttpServerSession> session, 
                                                  std::shared_ptr<HttpRequest> req, 
                                                  std::shared_ptr<HttpResponse> resp)->boost::asio::awaitable<void> {
        (void)session;
        (void)req;
        resp->add_header("Access-Control-Allow-Origin", "*");
        resp->add_header("Access-Control-Allow-Methods", "POST, PATCH, OPTIONS, GET, DELETE, HEAD");
        resp->add_header("Access-Control-Max-Age", "86400");
        std::vector<std::string> vheaders;
        auto & headers = req->get_headers();
        for (auto & p : headers) {
            vheaders.push_back(p.first);
        }
        vheaders.push_back("Content-Type");
        vheaders.push_back("If-Match");
        
        std::string allow_headers = boost::join(vheaders, ",");
        resp->add_header("Access-Control-Allow-Headers", allow_headers);
        if (!(co_await resp->write_header(200, "Ok"))) {
            resp->close();
            co_return;
        }
        resp->close();
        co_return;
    });
    if (!ret) {
        return false;
    }

    ret = on_get("/api/cut_off_stream", std::bind(&HttpApiServer::cut_off_stream, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
    if (!ret) {
        return false;
    }

    auto & static_file_server_cfg = Config::get_instance()->get_http_api_config().get_static_file_server_config();
    if (static_file_server_cfg.is_enabled()) {
        auto path_map = static_file_server_cfg.get_path_map();
        for (auto it = path_map.begin(); it != path_map.end(); it++) {
            ret = on_static_fs(it->first, it->second);
            CORE_INFO("register static file map:{} --> {}", it->first, it->second);
            if (!ret) {
                return false;
            }
        }
    }

    return true;
}

boost::asio::awaitable<void> HttpApiServer::get_api_version(std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp) {
    (void)session;
    (void)req;
    Json::Value root;
    root["version"] = VERSION_STR;
    std::string body = root.toStyledString();
    resp->add_header("Access-Control-Allow-Origin", "*");
    if (!(co_await resp->write_header(200, "OK"))) {
        resp->close();
        co_return;
    }

    bool ret = co_await resp->write_data((const uint8_t*)(body.data()), body.size());
    if (!ret) {
        resp->close();
        co_return;
    }

    resp->close();
    co_return;
}

boost::asio::awaitable<void> HttpApiServer::get_obj_count(std::shared_ptr<HttpServerSession> session, std::shared_ptr<HttpRequest> req, std::shared_ptr<HttpResponse> resp) {
    (void)session;
    (void)req;
    resp->add_header("Access-Control-Allow-Origin", "*");
    resp->add_header("Content-type", "application/json");
    if (!(co_await resp->write_header(200, "OK"))) {
        resp->close();
        co_return;
    }

    ObjViewer obj_viewer;
    Json::Value root;
    Json::Value v = obj_viewer.to_json();
    root["code"] = 0;
    root["data"] = v;
    std::string body = root.toStyledString();
    bool ret = co_await resp->write_data((const uint8_t*)(body.data()), body.size());
    if (!ret) {
        resp->close();
        co_return;
    }

    resp->close();
    co_return;
}

boost::asio::awaitable<void> HttpApiServer::get_domain_apps(std::shared_ptr<HttpServerSession> session, 
                                                            std::shared_ptr<HttpRequest> req, 
                                                            std::shared_ptr<HttpResponse> resp) {
    (void)session;
    (void)req;
    auto config = Config::get_instance();
    if (!config) {
        if (!(co_await resp->write_header(500, "Server Error"))) {
            resp->close();
        }
        co_return;
    }

    auto domain_confs = config->get_domain_confs();
    Json::Value jdomain_confs;
    for (auto it_domain = domain_confs.begin(); it_domain != domain_confs.end(); it_domain++) {
        jdomain_confs[it_domain->first] = it_domain->second->to_json();
    }
    Json::Value root;
    root["code"] = 0;
    root["data"] = jdomain_confs;
    root["msg"] = "";
    std::string body = root.toStyledString();
    resp->add_header("Content-type", "application/json");
    resp->add_header("Access-Control-Allow-Origin", "*");
    if (!(co_await resp->write_header(200, "OK"))) {
        resp->close();
        co_return;
    }

    bool ret = co_await resp->write_data((const uint8_t*)(body.data()), body.size());
    if (!ret) {
        resp->close();
        co_return;
    }

    resp->close();
    co_return;
}

boost::asio::awaitable<void> HttpApiServer::get_domain_streams(std::shared_ptr<HttpServerSession> session, 
                                                            std::shared_ptr<HttpRequest> req, 
                                                            std::shared_ptr<HttpResponse> resp) {
    (void)session;
    (void)req;
    Json::Value root;
    auto domain = req->get_query_param("domain");
    auto app_streams = SourceManager::get_instance().get_sources(domain);
    for (auto it_app = app_streams.begin(); it_app != app_streams.end(); it_app++) {
        Json::Value japp_streams;
        for (auto stream : it_app->second) {
            auto v = co_await stream.second->sync_to_json();
            japp_streams[stream.first] = v;
        }
        root[it_app->first] = japp_streams;
    }

    std::string body = root.toStyledString();
    resp->add_header("Content-type", "application/json");
    resp->add_header("Access-Control-Allow-Origin", "*");
    if (!(co_await resp->write_header(200, "OK"))) {
        resp->close();
        co_return;
    }

    bool ret = co_await resp->write_data((const uint8_t*)(body.data()), body.size());
    if (!ret) {
        resp->close();
        co_return;
    }

    resp->close();
    co_return;
}  

boost::asio::awaitable<void> HttpApiServer::get_app_streams(std::shared_ptr<HttpServerSession> session, 
                                                            std::shared_ptr<HttpRequest> req, 
                                                            std::shared_ptr<HttpResponse> resp) {
    (void)session;
    (void)req;
    Json::Value root;
    auto domain = req->get_query_param("domain");
    auto app = req->get_query_param("app");
    if (app.empty()) {
        co_return co_await response_json(resp, -1, "param error");
    }

    auto streams = SourceManager::get_instance().get_sources(domain, app);
    std::string body;
    try {
        Json::Value jstreams;
        for (auto it = streams.begin(); it != streams.end(); it++) {
            auto v = co_await it->second->sync_to_json();
            jstreams.append(v);
        }
        root["data"] = jstreams;
        root["code"] = 0;
        body = root.toStyledString();
    } catch (const std::exception &exp) {
        CORE_ERROR("Exception occurred: {}", exp.what());
    }
    
    resp->add_header("Content-type", "application/json");
    resp->add_header("Access-Control-Allow-Origin", "*");
    if (!(co_await resp->write_header(200, "OK"))) {
        resp->close();
        co_return;
    }

    bool ret = co_await resp->write_data((const uint8_t*)(body.data()), body.size());
    if (!ret) {
        resp->close();
        co_return;
    }

    resp->close();
    co_return;
} 

boost::asio::awaitable<void> HttpApiServer::get_domain_recorders(std::shared_ptr<HttpServerSession> session, 
                                                            std::shared_ptr<HttpRequest> req, 
                                                            std::shared_ptr<HttpResponse> resp) {
    (void)session;
    (void)req;
    Json::Value root;
    auto domain = req->get_query_param("domain");
    auto app_recorders = RecorderManager::get_instance().get_recorders(domain);
    for (auto it_app = app_recorders.begin(); it_app != app_recorders.end(); it_app++) {
        Json::Value japp_recorders;
        for (auto app_recorders : it_app->second) {
            for (auto r : app_recorders.second) {
                auto v = co_await r->sync_to_json();
                japp_recorders.append(v);
            }
        }
        root[it_app->first] = japp_recorders;
    }

    std::string body = root.toStyledString();
    resp->add_header("Content-type", "application/json");
    resp->add_header("Access-Control-Allow-Origin", "*");
    if (!(co_await resp->write_header(200, "OK"))) {
        resp->close();
        co_return;
    }

    bool ret = co_await resp->write_data((const uint8_t*)(body.data()), body.size());
    if (!ret) {
        resp->close();
        co_return;
    }

    resp->close();
    co_return;
}  

boost::asio::awaitable<void> HttpApiServer::get_app_recorders(std::shared_ptr<HttpServerSession> session, 
                                                            std::shared_ptr<HttpRequest> req, 
                                                            std::shared_ptr<HttpResponse> resp) {
    (void)session;
    (void)req;
    Json::Value root;
    auto domain = req->get_query_param("domain");
    auto app = req->get_query_param("app");
    if (app.empty()) {
        co_return co_await response_json(resp, -1, "param error");
    }

    auto recorders = RecorderManager::get_instance().get_recorders(domain, app);
    std::string body;
    try {
        Json::Value jrecorders;
        for (auto it = recorders.begin(); it != recorders.end(); it++) {
            for (auto r : it->second) {
                auto v = co_await r->sync_to_json();
                jrecorders.append(v);
            }
        }
        root["data"] = jrecorders;
        root["code"] = 0;
        body = root.toStyledString();
    } catch (const std::exception &exp) {
        CORE_ERROR("Exception occurred: {}", exp.what());
    }
    
    resp->add_header("Content-type", "application/json");
    resp->add_header("Access-Control-Allow-Origin", "*");
    if (!(co_await resp->write_header(200, "OK"))) {
        resp->close();
        co_return;
    }

    bool ret = co_await resp->write_data((const uint8_t*)(body.data()), body.size());
    if (!ret) {
        resp->close();
        co_return;
    }

    resp->close();
    co_return;
} 

boost::asio::awaitable<void> HttpApiServer::cut_off_stream(std::shared_ptr<HttpServerSession> session, 
                                                            std::shared_ptr<HttpRequest> req, 
                                                            std::shared_ptr<HttpResponse> resp) 
{
    (void)session;
    auto domain = req->get_query_param("domain");
    auto app = req->get_query_param("app");
    auto stream = req->get_query_param("stream");
    auto source = SourceManager::get_instance().get_source(domain, app, stream);
    if (!source) {
        co_return co_await response_json(resp, -1, "stream not found");
    }

    source->close();
    co_await response_json(resp, 0, "succeed");
    co_return;
}

struct StopRecorderParams {
    std::string domain;
    std::string app;
    std::string stream;
    std::string type;
    bool parse(const std::string & data) {
        Json::Value root;
        JSONCPP_STRING errs;
        Json::CharReaderBuilder readerBuilder;
        std::unique_ptr<Json::CharReader> const jsonReader(readerBuilder.newCharReader());
        if (!jsonReader->parse((char*)data.data(), (char*)data.data() + data.size(), &root, &errs)) {
            return false;
        }

        if (!errs.empty()) {
            return false;
        }

        if (!root.isMember("domain") || !root["domain"].isString() ||
            !root.isMember("app") || !root["app"].isString() ||
            !root.isMember("stream") || !root["stream"].isString() ||
            !root.isMember("type") || !root["type"].isString()) {
            return false;
        }

        domain = root["domain"].asString();
        app = root["app"].asString();
        stream = root["stream"].asString();
        type = root["type"].asString();
        return true;
    }
};

boost::asio::awaitable<void> HttpApiServer::stop_recorder(std::shared_ptr<HttpServerSession> session, 
                                                            std::shared_ptr<HttpRequest> req, 
                                                            std::shared_ptr<HttpResponse> resp) 
{
    (void)session;
    StopRecorderParams param;
    if (!param.parse(req->get_body())) {
        co_await response_json(resp, -1, "error param");
        co_return;
    }

    auto srecorder = RecorderManager::get_instance().get_recorder(param.domain, param.app, param.stream);
    if (srecorder.size() <= 0) {
        co_return co_await response_json(resp, -1, "stream not found");
    }

    for (auto r : srecorder) {
        if (r->type() == param.type) {
            r->close();
            break;
        }
    }
    
    co_await response_json(resp, 0, "succeed");
    co_return;
}


boost::asio::awaitable<void> HttpApiServer::response_json(std::shared_ptr<HttpResponse> resp, int32_t code, const std::string & msg) {
    Json::Value root;
    root["code"] = code;
    root["msg"] = msg;
    std::string body = root.toStyledString();
    resp->add_header("Access-Control-Allow-Origin", "*");
    resp->add_header("Content-Length", std::to_string(body.size()));
    if (!(co_await resp->write_header(200, "OK"))) {
        resp->close();
        co_return;
    }

    bool ret = co_await resp->write_data((const uint8_t*)(body.data()), body.size());
    if (!ret) {
        resp->close();
        co_return;
    }

    resp->close();
    co_return;
}