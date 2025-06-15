/*
 * @Author: jbl19860422
 * @Date: 2023-12-26 22:39:43
 * @LastEditTime: 2023-12-27 22:08:17
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\transcode\rtmp_to_webrtc.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <boost/asio/steady_timer.hpp>

#include "core/rtmp_media_sink.hpp"
#include "core/webrtc_media_source.hpp"
#include "../media_bridge.hpp"
#include "protocol/sdp/sdp.hpp"
#include "protocol/rtp/rtp_packer.h"
#include "base/wait_group.h"
extern "C" {
    #include "libswresample/swresample.h"
}
#include "base/obj_tracker.hpp"

namespace mms {
class WebRtcMediaSource;
class RtmpMetaDataMessage;
class Codec;
class PublishApp;
class RtmpMessage;
class AACDecoder;
class MOpusEncoder;

class RtmpToWebRtc : public MediaBridge, public ObjTracker<RtmpToWebRtc> {
public:
    RtmpToWebRtc(ThreadWorker *worker, std::shared_ptr<PublishApp>, std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name);
    virtual ~RtmpToWebRtc();
    bool init() override;
    boost::asio::awaitable<bool> on_audio_packet(std::shared_ptr<RtmpMessage> audio_pkt);
    boost::asio::awaitable<bool> on_video_packet(std::shared_ptr<RtmpMessage> video_pkt);
    boost::asio::awaitable<bool> on_metadata(std::shared_ptr<RtmpMessage> metadata_pkt);
    void close() override;
protected:
    int32_t get_nalus(uint8_t *data, int32_t len, std::list<std::string_view> & nalus);

    std::shared_ptr<RtmpMediaSink> rtmp_media_sink_;
    std::shared_ptr<WebRtcMediaSource> webrtc_media_source_;
    
    std::shared_ptr<RtmpMetaDataMessage> metadata_;
    std::shared_ptr<RtmpMessage> video_header_;
    std::shared_ptr<RtmpMessage> audio_header_;
    int32_t nalu_length_size_ = 4;

    bool has_audio_ = false;
    bool has_video_ = false;
    bool audio_ready_ = false;
    bool video_ready_ = false;

    std::shared_ptr<Codec> video_codec_;
    std::shared_ptr<Codec> audio_codec_;
    uint16_t video_seq_ = 0;
    uint16_t audio_seq_ = 0;

    std::shared_ptr<Sdp> play_offer_sdp_;
    uint8_t audio_pt_ = 111;
    uint8_t video_pt_ = 127;
    uint32_t audio_ssrc_ = 1322989171;
    uint32_t video_ssrc_ = 3353359166;

    int64_t first_video_pts_ = 0;
    int64_t first_audio_pts_ = 0;

    boost::asio::steady_timer check_closable_timer_;

    RtpPacker video_rtp_packer_;
    RtpPacker audio_rtp_packer_;
    std::shared_ptr<AACDecoder> aac_decoder_;
    SwrContext *swr_context_ = nullptr;
    uint8_t resampled_pcm_[8192*2];
    int32_t resampled_pcm_bytes_ = 0;
    std::shared_ptr<MOpusEncoder> opus_encoder_;
    uint8_t opus_data_[8192*2];
    int32_t audio_sample_count_ = 0;

    WaitGroup wg_;
};
};