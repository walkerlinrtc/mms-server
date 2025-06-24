#pragma once

#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>

#include "core/stream_session.hpp"
#include "base/wait_group.h"
#include "core/source_status.h"
#include "base/obj_tracker.hpp"

namespace mms {
class RtmpMediaSink;
class HttpRequest;
class HttpResponse;
class ThreadWorker;

class HttpM3u8ServerSession : public StreamSession, public ObjTracker<HttpM3u8ServerSession> {
public:
    HttpM3u8ServerSession(std::shared_ptr<HttpRequest> http_req, std::shared_ptr<HttpResponse> http_resp);
    virtual ~HttpM3u8ServerSession();
    void start();
    void stop();
private:
    boost::asio::awaitable<bool> process_source_status(SourceStatus status);
    std::shared_ptr<RtmpMediaSink> rtmp_media_sink_;
    std::shared_ptr<HttpRequest> http_request_;
    std::shared_ptr<HttpResponse> http_response_;
};
};