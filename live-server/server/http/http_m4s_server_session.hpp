#pragma once
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>

#include "core/stream_session.hpp"
#include "base/wait_group.h"
namespace mms {
class HttpRequest;
class HttpResponse;
class RtmpMediaSink;
class ThreadWorker;
class HttpM4sServerSession : public StreamSession {
public:
    HttpM4sServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp);
    virtual ~HttpM4sServerSession();
    void start();
    void stop();
private:
    std::shared_ptr<RtmpMediaSink> rtmp_media_sink_;
    std::shared_ptr<HttpRequest> http_request_;
    std::shared_ptr<HttpResponse> http_response_;
    std::atomic_flag closed_ = ATOMIC_FLAG_INIT;
};
};