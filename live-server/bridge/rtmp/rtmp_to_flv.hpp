/*
 * @Author: jbl19860422
 * @Date: 2023-12-02 21:39:27
 * @LastEditTime: 2023-12-27 22:03:04
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\transcode\rtmp_to_flv.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <boost/asio/steady_timer.hpp>

#include "core/rtmp_media_sink.hpp"
#include "core/flv_media_source.hpp"

#include "base/wait_group.h"
#include "../media_bridge.hpp"
#include "base/obj_tracker.hpp"

namespace mms {
class FlvMediaSource;
class PublishApp;
class RtmpToFlv : public MediaBridge, public ObjTracker<RtmpToFlv> {
public:
    RtmpToFlv(ThreadWorker *worker, std::shared_ptr<PublishApp>, std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name);
    virtual ~RtmpToFlv();
    bool init() override;
    bool on_metadata(std::shared_ptr<RtmpMessage> metadata_pkt);
    virtual bool on_audio_packet(std::shared_ptr<RtmpMessage> audio_pkt);
    virtual bool on_video_packet(std::shared_ptr<RtmpMessage> video_pkt);
    void close();    
protected:
    boost::asio::steady_timer check_closable_timer_;
    std::shared_ptr<RtmpMediaSink> rtmp_media_sink_;
    std::shared_ptr<FlvMediaSource> flv_media_source_;
    WaitGroup wg_;
};
};