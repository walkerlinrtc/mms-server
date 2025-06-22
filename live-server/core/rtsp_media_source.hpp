/*
 * @Author: jbl19860422
 * @Date: 2023-07-02 10:56:58
 * @LastEditTime: 2023-12-27 14:25:55
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\rtsp_media_source.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <string>
#include <memory>

#include <boost/asio/awaitable.hpp>

#include "json/json.h"
#include "core/rtp_media_source.hpp"
#include "base/obj_tracker.hpp"

namespace mms {
class ThreadWorker;
class HlsProducer;
class Sdp;
class MediaSdp;
class Payload;
class StreamSession;
class PublishApp;
class MediaBridge;

class RtspMediaSource : public RtpMediaSource, public ObjTracker<RtspMediaSource> {
public:
    RtspMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> s, std::shared_ptr<PublishApp> app);
    virtual ~RtspMediaSource();
    bool process_announce_sdp(const std::string & sdp);

    Json::Value to_json() override;
    uint16_t get_video_pt() const {
        return video_pt_;
    }

    uint16_t get_audio_pt() const {
        return audio_pt_;
    }

    bool create_video_codec_by_sdp(const MediaSdp & sdp, const Payload & payload);
    bool create_audio_codec_by_sdp(const MediaSdp & sdp, const Payload & payload);

    std::shared_ptr<Sdp> get_announce_sdp() {
        return announce_sdp_;
    }

    void set_announce_sdp(std::shared_ptr<Sdp> sdp) {
        announce_sdp_ = sdp;
    }

    void set_audio_pt(uint16_t v) {
        audio_pt_ = v;
    }

    void set_video_pt(uint16_t v) {
        video_pt_ = v;
    }
    
    std::shared_ptr<MediaBridge> get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, const std::string & stream_name);
private:
    Payload* find_suitable_video_payload(MediaSdp & media_sdp);
    Payload* find_suitable_audio_payload(MediaSdp & media_sdp);
    void on_stream_ready();
    std::shared_ptr<Sdp> announce_sdp_;
    uint16_t video_pt_ = 0;
    uint16_t audio_pt_ = 0;
};

};