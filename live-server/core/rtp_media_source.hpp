/*
 * @Author: jbl19860422
 * @Date: 2023-12-26 22:24:33
 * @LastEditTime: 2023-12-27 21:16:49
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\rtp_media_source.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <vector>
#include <set>
#include <map>
#include <boost/circular_buffer.hpp>
#include <boost/asio/awaitable.hpp>

#include "media_source.hpp"
#include "media_sink.hpp"

#include "base/thread/thread_worker.hpp"
#include "base/sequence_pkt_buf.hpp"

#include "protocol/rtp/rtp_packet.h"
#include "protocol/rtp/rtp_h264_depacketizer.h"

namespace mms {
class Session;
class PublishApp;
class RtpMediaSource : public MediaSource {
public:
    RtpMediaSource(const std::string & media_type, std::weak_ptr<StreamSession> session, std::shared_ptr<PublishApp> app, ThreadWorker *worker);
    virtual ~RtpMediaSource();

    bool init();
    virtual boost::asio::awaitable<bool> on_audio_packets(std::vector<std::shared_ptr<RtpPacket>> audio_pkts);
    virtual boost::asio::awaitable<bool> on_video_packets(std::vector<std::shared_ptr<RtpPacket>> video_pkts);
    virtual boost::asio::awaitable<bool> on_source_codec_ready();

    std::shared_ptr<MediaBridge> get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, const std::string & stream_name);
protected:
    boost::circular_buffer<std::shared_ptr<RtpPacket>> av_pkts_;
    std::map<uint16_t, std::shared_ptr<RtpPacket>> video_pkts_;
    std::map<uint16_t, std::shared_ptr<RtpPacket>> audio_pkts_;
protected:
    bool has_video_; 
    bool video_ready_ = false;
    bool has_audio_;
    bool audio_ready_ = false;
    int32_t latest_video_timestamp_ = 0;
    int32_t latest_audio_timestamp_ = 0;

    RtpH264Depacketizer h264_depacketizer_;
};
};