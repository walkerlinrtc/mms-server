#pragma once
#include <memory>
#include <functional>
#include "http_server_base.hpp"
namespace mms {
class WebRtcServer;
class HttpApiServer : public HttpServerBase {
public:
    HttpApiServer(ThreadWorker *w):HttpServerBase(w) {
    }
    
    virtual ~HttpApiServer();
private:
    boost::asio::awaitable<void> response_json(std::shared_ptr<HttpResponse> resp, int32_t code, const std::string & msg);
    bool register_route();
private:
    boost::asio::awaitable<void> get_api_version(std::shared_ptr<HttpServerSession> session, 
                                                 std::shared_ptr<HttpRequest> req, 
                                                 std::shared_ptr<HttpResponse> resp);

    boost::asio::awaitable<void> get_domain_streams(std::shared_ptr<HttpServerSession> session, 
                                                    std::shared_ptr<HttpRequest> req, 
                                                    std::shared_ptr<HttpResponse> resp);

    boost::asio::awaitable<void> get_app_streams(std::shared_ptr<HttpServerSession> session, 
                                                 std::shared_ptr<HttpRequest> req, 
                                                 std::shared_ptr<HttpResponse> resp);

    boost::asio::awaitable<void> cut_off_stream(std::shared_ptr<HttpServerSession> session, 
                                                 std::shared_ptr<HttpRequest> req, 
                                                 std::shared_ptr<HttpResponse> resp);
};
};