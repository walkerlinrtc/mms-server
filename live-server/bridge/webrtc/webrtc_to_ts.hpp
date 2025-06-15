/*
 * @Author: jbl19860422
 * @Date: 2023-08-31 23:19:56
 * @LastEditTime: 2023-11-07 20:50:53
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\transcode\rtmp_to_ts.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once 
#include <vector>
#include <list>
#include <memory>
#include <shared_mutex>

#include "protocol/rtp/rtp_h264_depacketizer.h"
#include "protocol/rtp/rtp_aac_depacketizer.h"

#include "protocol/rtmp/rtmp_define.hpp"
#include "bridge/media_bridge.hpp"
#include "core/rtmp_media_sink.hpp"
#include "core/ts_media_source.hpp"
#include "protocol/ts/ts_pat_pmt.hpp"
#include "../rtmp/rtmp_to_ts.hpp"
#include "protocol/rtp/rtp_packet.h"
extern "C" {
    #include "libswresample/swresample.h"
}
#include "opus/opus.h"
#include "base/wait_group.h"
#include "base/obj_tracker.hpp"

namespace mms {
class RtmpMetaDataMessage;
class Codec;
class RtpMediaSink;
class TsSegment;
class RtpPacket;
class ThreadWorker;
class PublishApp;
class RtpPacket;
class RtpMediaSink;
class FlvMediaSource;
class Codec;
class RtmpMetaDataMessage;
class RtpH264NALU;
class RtpAACNALU;
class AACEncoder;

class WebRtcToTs : public MediaBridge, public ObjTracker<WebRtcToTs> {
public:
    WebRtcToTs(ThreadWorker *worker, std::shared_ptr<PublishApp> app, std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name);
    virtual ~WebRtcToTs();
public:
    bool init() override;
    void process_video_packet(std::shared_ptr<RtpPacket> pkt);
    void process_h264_packet(std::shared_ptr<RtpPacket> pkt);
    void process_audio_packet(std::shared_ptr<RtpPacket> pkt, int64_t timestamp);
    void on_ts_segment(std::shared_ptr<TsSegment> ts_seg);
    void close() override;
private:
    std::shared_ptr<TsSegment> curr_seg_;
private:
    std::shared_ptr<TsMediaSource> ts_media_source_;
    std::shared_ptr<RtpMediaSink> rtp_media_sink_;

    bool has_video_ = false;
    bool video_ready_ = false;
    bool has_audio_ = false;
    bool audio_ready_ = false;
    std::shared_ptr<Codec> video_codec_;
    std::shared_ptr<Codec> audio_codec_;
    
    int16_t audio_pid_ = -1;
    int16_t video_pid_ = -1;
    int16_t PCR_PID = -1;
    TsStream video_type_;
    TsStream audio_type_;
    void generate_h264_ts(int64_t timestamp, std::shared_ptr<RtpH264NALU> & nalu);
    void process_opus_packet(std::shared_ptr<RtpPacket> pkt, int64_t timestamp);
    void create_pat(std::string_view & pat_seg);
    void create_pmt(std::string_view & pmt_seg);
    void create_video_ts(std::shared_ptr<PESPacket> pes_packet, int32_t pes_len, bool is_key);
    void create_audio_ts(std::shared_ptr<PESPacket> pes_packet);
    std::unordered_map<int16_t, uint8_t> continuity_counter_;

    boost::asio::steady_timer check_closable_timer_;
    char video_pes_header_[256];
    std::vector<std::string_view> video_pes_segs_;

    int32_t adts_header_index_ = 0;
    std::vector<AdtsHeader> adts_headers_;
    char audio_pes_header_[256];
    AudioBuff<RtpPacket> audio_buf_;

    std::unique_ptr<char[]> video_frame_cache_;
    std::unique_ptr<char[]> audio_frame_cache_;

    int64_t first_rtp_video_ts_ = 0;
    int64_t first_rtp_audio_ts_ = 0;

    bool wait_first_key_frame_ = true;
    RtpH264Depacketizer rtp_h264_depacketizer_;
    std::unique_ptr<char[]> aac_buf_;
    int32_t aac_buf_bytes_ = 0;

    std::shared_ptr<OpusDecoder> opus_decoder_;
    SwrContext *swr_context_ = nullptr;
    opus_int16 decoded_pcm_[8192];
    uint8_t resampled_pcm_[8192];
    uint8_t aac_data_[8192];
    int32_t aac_bytes_ = 0;
    int32_t resampled_pcm_samples_ = 0;
    std::unique_ptr<AACEncoder> aac_encoder_;

    WaitGroup wg_;
};
};