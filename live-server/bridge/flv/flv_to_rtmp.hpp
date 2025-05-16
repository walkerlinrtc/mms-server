/*
 * @Author: jbl19860422
 * @Date: 2023-12-26 22:39:43
 * @LastEditTime: 2023-12-27 18:59:02
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\transcode\flv_to_rtmp.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <atomic>
#include <boost/asio/steady_timer.hpp>

#include "../media_bridge.hpp"
#include "base/wait_group.h"

namespace mms {
class RtmpMediaSource;
class FlvMediaSink;
class FlvTag;
class PublishApp;
class ThreadWorker;
class FlvToRtmp : public MediaBridge {
public:
    FlvToRtmp(ThreadWorker *worker, std::shared_ptr<PublishApp>, std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name);
    virtual ~FlvToRtmp();
    bool init() override;
    bool on_metadata(std::shared_ptr<FlvTag> metadata_pkt);
    virtual bool on_audio_packet(std::shared_ptr<FlvTag> audio_pkt);
    virtual bool on_video_packet(std::shared_ptr<FlvTag> video_pkt);
    void close() override;
protected:
    std::shared_ptr<FlvMediaSink> flv_media_sink_;
    std::shared_ptr<RtmpMediaSource> rtmp_media_source_;
    boost::asio::steady_timer check_closable_timer_;

    WaitGroup wg_;
};
};