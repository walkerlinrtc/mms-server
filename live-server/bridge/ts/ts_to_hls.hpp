#pragma once
#include <atomic>
#include <memory>

#include <boost/asio/steady_timer.hpp>

#include "../media_bridge.hpp"
#include "base/wait_group.h"

namespace mms {
class PublishApp;
class TsMediaSink;
class ThreadWorker;
class HlsLiveMediaSource;

class TsToHls : public MediaBridge {
public:
    TsToHls(ThreadWorker *worker, std::shared_ptr<PublishApp> app, std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name);
    virtual ~TsToHls();
    bool init() override;
    void close() override;
private:
    std::shared_ptr<TsMediaSink> ts_media_sink_;
    std::shared_ptr<HlsLiveMediaSource> hls_media_source_;

    boost::asio::steady_timer check_closable_timer_;

    WaitGroup wg_;
};
};