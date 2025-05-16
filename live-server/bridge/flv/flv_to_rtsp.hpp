/*
 * @Author: jbl19860422
 * @Date: 2023-12-26 22:24:34
 * @LastEditTime: 2023-12-27 19:02:00
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\transcode\flv_to_rtsp.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <memory>
#include <list>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/awaitable.hpp>

#include "../media_bridge.hpp"
#include "protocol/rtp/rtp_packer.h"
#include "base/wait_group.h"

namespace mms {
class ThreadWorker;
class FlvMediaSink;
class RtspMediaSource;
class RtmpMessage;
class RtmpMetaDataMessage;
class Codec;
class PublishApp;
class Sdp;
class FlvTag;
class FlvToRtsp : public MediaBridge {
public:
    FlvToRtsp(ThreadWorker *worker, std::shared_ptr<PublishApp>, std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name);
    virtual ~FlvToRtsp();
    // 主要处理函数
    bool init() override;
    boost::asio::awaitable<bool> on_audio_packet(std::shared_ptr<FlvTag> audio_pkt);
    boost::asio::awaitable<bool> on_video_packet(std::shared_ptr<FlvTag> video_pkt);
    boost::asio::awaitable<bool> on_metadata(std::shared_ptr<FlvTag> metadata_pkt);
    void close() override;
private:
    bool generate_sdp();

    std::shared_ptr<FlvMediaSink> flv_media_sink_;
    std::shared_ptr<RtspMediaSource> rtsp_media_source_;

    int32_t get_nalus(uint8_t *data, int32_t len, std::list<std::string_view> & nalus);
    std::shared_ptr<RtmpMetaDataMessage> metadata_;
    std::shared_ptr<FlvTag> video_header_;
    std::shared_ptr<FlvTag> audio_header_;
    bool has_video_ = false;
    bool video_ready_ = false;
    bool has_audio_ = false;
    bool audio_ready_ = false;
    std::shared_ptr<Codec> video_codec_;
    std::shared_ptr<Codec> audio_codec_;
    int32_t nalu_length_size_ = 4;
    
    boost::asio::steady_timer check_closable_timer_;

    std::shared_ptr<Sdp> sdp_;
    uint8_t audio_pt_ = 97;
    uint8_t video_pt_ = 96;

    int64_t first_video_pts_ = 0;
    int64_t first_audio_pts_ = 0;

    RtpPacker video_rtp_packer_;
    RtpPacker audio_rtp_packer_;

    uint8_t audio_buf_[8192];
    int32_t audio_buf_bytes_ = 0;
    WaitGroup wg_;
private:
    boost::asio::awaitable<bool> process_h264_packet(std::shared_ptr<FlvTag> video_pkt);
    boost::asio::awaitable<bool> process_h265_packet(std::shared_ptr<FlvTag> video_pkt);
    boost::asio::awaitable<bool> process_aac_packet(std::shared_ptr<FlvTag> audio_pkt);
};
};