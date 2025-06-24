#pragma once
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>

#include "core/stream_session.hpp"
#include "base/wait_group.h"
#include "base/obj_tracker.hpp"
namespace mms {
class HttpRequest;
class HttpResponse;
class RtmpMediaSink;
class ThreadWorker;
class HttpTsServerSession : public StreamSession, public ObjTracker<HttpTsServerSession> {
public:
    HttpTsServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp);
    virtual ~HttpTsServerSession();
    void start();
    void stop();
private:
    std::shared_ptr<RtmpMediaSink> rtmp_media_sink_;
    std::shared_ptr<HttpRequest> http_request_;
    std::shared_ptr<HttpResponse> http_response_;
    std::atomic_flag closed_ = ATOMIC_FLAG_INIT;
};
};