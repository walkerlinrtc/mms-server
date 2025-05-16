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

#include <boost/asio/steady_timer.hpp>

#include "rtmp_define.hpp"
#include "../media_bridge.hpp"
#include "core/rtmp_media_sink.hpp"
#include "core/ts_media_source.hpp"
#include "protocol/ts/ts_pat_pmt.hpp"
#include "base/wait_group.h"

namespace mms {
class RtmpMetaDataMessage;
class Codec;
class PublishApp;
class TsSegment;
class PESPacket;

template<typename T>
struct AudioBuff {
    AudioBuff() {
        timestamp = 0;
        audio_pes_len = 0;
        audio_pkts.reserve(20);
        audio_pes_segs.reserve(20);
        audio_pes_segs.push_back(std::string_view(nullptr, 0));// 第一个，总留给pes header
    }

    void clear() {
        timestamp = 0;
        audio_pes_len = 0;
        audio_pkts.clear();
        audio_pes_segs.clear();
        audio_pes_segs.push_back(std::string_view(nullptr, 0));
    }

    std::vector<std::shared_ptr<T>> audio_pkts;
    std::vector<std::string_view> audio_pes_segs;
    int32_t audio_pes_len = 0;
    int64_t timestamp = 0;
};

struct AdtsHeader {
    char data[7];
};

class RtmpToTs : public MediaBridge {
public:
    RtmpToTs(ThreadWorker *worker, std::shared_ptr<PublishApp> app, std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name);
    virtual ~RtmpToTs();
public:
    bool init() override;
    bool on_metadata(std::shared_ptr<RtmpMessage> metadata_pkt);
    bool on_video_packet(std::shared_ptr<RtmpMessage> video_pkt);
    bool on_audio_packet(std::shared_ptr<RtmpMessage> audio_pkt);
    void on_ts_segment(std::shared_ptr<TsSegment> ts_seg);
    void close() override;
private:
    bool process_h264_packet(std::shared_ptr<RtmpMessage> video_pkt);
    bool process_h265_packet(std::shared_ptr<RtmpMessage> video_pkt);
    bool process_aac_packet(std::shared_ptr<RtmpMessage> audio_pkt);
    bool process_mp3_packet(std::shared_ptr<RtmpMessage> audio_pkt);
    std::shared_ptr<TsSegment> curr_seg_;
private:
    int32_t get_nalus(uint8_t *data, int32_t len, std::list<std::string_view> & nalus);

    std::shared_ptr<RtmpMediaSink> rtmp_media_sink_;
    std::shared_ptr<TsMediaSource> ts_media_source_;
    
    std::shared_ptr<RtmpMetaDataMessage> metadata_;
    std::shared_ptr<RtmpMessage> video_header_;
    std::shared_ptr<RtmpMessage> audio_header_;
    bool has_video_ = false;
    bool video_ready_ = false;
    bool has_audio_ = false;
    bool audio_ready_ = false;
    std::shared_ptr<Codec> video_codec_;
    std::shared_ptr<Codec> audio_codec_;
    int32_t nalu_length_size_ = 4;
    int16_t audio_pid_ = -1;
    int16_t video_pid_ = -1;
    int16_t PCR_PID = -1;
    TsStream video_type_;
    TsStream audio_type_;
    void create_pat(std::string_view & pat_seg);
    void create_pmt(std::string_view & pmt_seg);
    void create_video_ts(std::shared_ptr<PESPacket> video_pkt, int32_t pes_len, bool is_key);
    void create_audio_ts(std::shared_ptr<PESPacket> audio_pkt);
    std::unordered_map<int16_t, uint8_t> continuity_counter_;

    boost::asio::steady_timer check_closable_timer_;
    char video_pes_header_[256];
    std::vector<std::string_view> video_pes_segs_;

    int32_t adts_header_index_ = 0;
    std::vector<AdtsHeader> adts_headers_;
    char audio_pes_header_[256];
    AudioBuff<RtmpMessage> audio_buf_;

    WaitGroup wg_;
};
};