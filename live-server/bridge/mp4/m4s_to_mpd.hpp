#pragma once
#include <atomic>
#include <memory>

#include <boost/asio/steady_timer.hpp>

#include "../media_bridge.hpp"
#include "base/wait_group.h"
#include "base/obj_tracker.hpp"

namespace mms {
class PublishApp;
class M4sMediaSink;
class ThreadWorker;
class MpdLiveMediaSource;

class M4sToMpd : public MediaBridge, public ObjTracker<M4sToMpd> {
public:
    M4sToMpd(ThreadWorker *worker, std::shared_ptr<PublishApp> app, std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name);
    virtual ~M4sToMpd();
    bool init() override;
    void close() override;
private:
    std::shared_ptr<M4sMediaSink> mp4_media_sink_;
    std::shared_ptr<MpdLiveMediaSource> mpd_media_source_;

    boost::asio::steady_timer check_closable_timer_;

    WaitGroup wg_;
};
};