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
#include "core/mp4_media_source.hpp"
#include "protocol/ts/ts_pat_pmt.hpp"
#include "base/wait_group.h"
#include "mp4/mp4_builder.h"
#include "mp4/mp4_segment.h"

namespace mms {
class RtmpMetaDataMessage;
class Codec;
class PublishApp;
class Mp4Segment;
class MoovBox;
class Mp4MediaSource;

class RtmpToMp4 : public MediaBridge {
public:
    RtmpToMp4(ThreadWorker *worker, std::shared_ptr<PublishApp> app, std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name);
    virtual ~RtmpToMp4();
public:
    bool init() override;
    bool on_metadata(std::shared_ptr<RtmpMessage> metadata_pkt);
    bool on_video_packet(std::shared_ptr<RtmpMessage> video_pkt);
    bool on_audio_packet(std::shared_ptr<RtmpMessage> audio_pkt);
    void close() override;
private:
    bool process_h264_packet(std::shared_ptr<RtmpMessage> video_pkt);
    bool process_h265_packet(std::shared_ptr<RtmpMessage> video_pkt);
    bool generate_video_init_seg(std::shared_ptr<RtmpMessage> video_pkt);
    bool generate_audio_init_seg(std::shared_ptr<RtmpMessage> audio_pkt);
    
    bool process_aac_packet(std::shared_ptr<RtmpMessage> audio_pkt);
    bool process_mp3_packet(std::shared_ptr<RtmpMessage> audio_pkt);
    void reap_video_seg(int64_t dts);
    void reap_audio_seg(int64_t dts);
private:
    int32_t get_nalus(uint8_t *data, int32_t len, std::list<std::string_view> & nalus);

    std::shared_ptr<RtmpMediaSink> rtmp_media_sink_;
    std::shared_ptr<Mp4MediaSource> mp4_media_source_;
    
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

    int16_t video_track_ID_ = 1;
    int16_t audio_track_ID_ = 2;
    
    std::shared_ptr<MoovBox> video_moov_;
    std::shared_ptr<MoovBox> audio_moov_;

    boost::asio::steady_timer check_closable_timer_;

    std::shared_ptr<Mp4Segment> init_video_mp4_seg_;
    std::shared_ptr<Mp4Segment> init_audio_mp4_seg_;
    
    std::vector<std::shared_ptr<RtmpMessage>> video_pkts_;
    int64_t video_bytes_ = 0;
    std::vector<std::shared_ptr<RtmpMessage>> audio_pkts_;
    int64_t audio_bytes_ = 0;
    std::shared_ptr<Mp4Segment> video_data_mp4_seg_;
    std::shared_ptr<Mp4Segment> audio_data_mp4_seg_;

    uint64_t video_seq_no_ = 1;
    uint64_t audio_seq_no_ = 1;
    bool video_reaped_ = false;
    WaitGroup wg_;
};
};