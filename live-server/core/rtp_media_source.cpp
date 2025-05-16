/*
 * @Author: jbl19860422
 * @Date: 2023-08-27 09:48:03
 * @LastEditTime: 2023-12-29 15:17:48
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\rtp_media_source.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include "spdlog/spdlog.h"
#include <iostream>
#include "rtp_media_source.hpp"
#include "rtp_media_sink.hpp"
#include "bridge/media_bridge.hpp"
#include "core/stream_session.hpp"

using namespace mms;

RtpMediaSource::RtpMediaSource(const std::string & media_type, std::weak_ptr<StreamSession> session, std::shared_ptr<PublishApp> app, ThreadWorker *worker) : MediaSource(media_type, session, app, worker) {

}

bool RtpMediaSource::init() {
    return true;
}

RtpMediaSource::~RtpMediaSource() {

}

boost::asio::awaitable<bool> RtpMediaSource::on_audio_packets(std::vector<std::shared_ptr<RtpPacket>> audio_pkts) 
{    
    std::lock_guard<std::recursive_mutex> lck(sinks_mtx_);
    for (auto & sink : sinks_) {
        co_await (std::static_pointer_cast<RtpMediaSink>(sink))->on_audio_packets(audio_pkts);
    }

    co_return true;
}

boost::asio::awaitable<bool> RtpMediaSource::on_video_packets(std::vector<std::shared_ptr<RtpPacket>> video_pkts) 
{
    std::lock_guard<std::recursive_mutex> lck(sinks_mtx_);
    for (auto & sink : sinks_) {
        co_await (std::static_pointer_cast<RtpMediaSink>(sink))->on_video_packets(video_pkts);
    }
    co_return true;
}

boost::asio::awaitable<bool> RtpMediaSource::on_source_codec_ready() {
    std::shared_lock<std::shared_mutex> lck(bridges_mtx_);
    for (auto & p : bridges_) {
        auto sink = std::static_pointer_cast<RtpMediaSink>(p.second->get_media_sink());
        sink->on_source_codec_ready(video_codec_, audio_codec_);
    }
    co_return true;
}

std::shared_ptr<MediaBridge> RtpMediaSource::get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, const std::string &stream_name) {
    ((void)id);
    ((void)app);
    ((void)stream_name);
    return nullptr;
}