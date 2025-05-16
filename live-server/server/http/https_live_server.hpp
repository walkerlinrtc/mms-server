#pragma once
#include <memory>
#include "https_server_base.hpp"
namespace mms {
class WebRtcServer;

class HttpsLiveServer : public HttpsServerBase {
public:
    HttpsLiveServer(ThreadWorker *w);
    virtual ~HttpsLiveServer();
    std::shared_ptr<SSL_CTX> on_tls_ext_servername(const std::string & domain_name) override;
    void set_webrtc_server(std::shared_ptr<WebRtcServer> wrtc_server);
private:
    bool register_route();
private:
    std::shared_ptr<WebRtcServer> webrtc_server_;
};
};