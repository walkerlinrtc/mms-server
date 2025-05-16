/*
 * @Author: jbl19860422
 * @Date: 2023-09-17 11:27:45
 * @LastEditTime: 2023-09-19 23:53:51
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\flv_media_sink.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include "flv_media_sink.hpp"
#include "flv_media_source.hpp"

#include "protocol/rtmp/rtmp_define.hpp"
#include "protocol/rtmp/rtmp_message/data_message/rtmp_metadata_message.hpp"

#include "base/thread/thread_worker.hpp"
#include "base/sequence_pkt_buf.hpp"
#include "spdlog/spdlog.h"

using namespace mms;

FlvMediaSink::FlvMediaSink(ThreadWorker *worker) : LazyMediaSink(worker) {
}

bool FlvMediaSink::init() {
    return true;
}

FlvMediaSink::~FlvMediaSink() {
    spdlog::info("destroy FlvMediaSink");
}

bool FlvMediaSink::on_audio_packet(std::shared_ptr<FlvTag> audio_pkt) {
    auto self(this->shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self, audio_pkt]()->boost::asio::awaitable<void> {
        std::vector<std::shared_ptr<FlvTag>> pkts{audio_pkt};
        if (!co_await cb_(pkts)) {
            co_return;
        }
        co_return;
    }, boost::asio::detached);
    return true;
}

bool FlvMediaSink::on_video_packet(std::shared_ptr<FlvTag> video_pkt) {
    auto self(this->shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self, video_pkt]()->boost::asio::awaitable<void> {
        std::vector<std::shared_ptr<FlvTag>> pkts{video_pkt};
        if (!co_await cb_(pkts)) {
            co_return;
        }
        co_return;
    }, boost::asio::detached);
    return true;
}

boost::asio::awaitable<bool> FlvMediaSink::send_flv_tag(std::shared_ptr<FlvTag> pkt) {
    ((void)pkt);
    co_return true;
}

bool FlvMediaSink::on_metadata(std::shared_ptr<FlvTag> metadata_pkt) {
    auto self(this->shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self, metadata_pkt]()->boost::asio::awaitable<void> {
        std::vector<std::shared_ptr<FlvTag>> pkts{metadata_pkt};
        if (!co_await cb_(pkts)) {
            co_return;
        }
        co_return;
    }, boost::asio::detached);
    return true;
} 

boost::asio::awaitable<void> FlvMediaSink::do_work() {
    if (!source_->is_stream_ready()) {
        co_return;
    }

    std::vector<std::shared_ptr<FlvTag>> pkts;
    pkts.reserve(20);
    auto flv_source = std::static_pointer_cast<FlvMediaSource>(source_);
    do {
        pkts = flv_source->get_pkts(last_send_pkt_index_, 20);
        if (!co_await cb_(pkts)) {
            break;
        }
    } while (pkts.size() > 0);
    co_return;
}

bool FlvMediaSink::on_source_codec_ready(std::shared_ptr<Codec> video_codec, std::shared_ptr<Codec> audio_codec) {
    bool ret = ready_cb_(video_codec, audio_codec);
    return ret;
}

void FlvMediaSink::set_source_codec_ready_cb(const std::function<bool(std::shared_ptr<Codec> video_codec, std::shared_ptr<Codec> audio_codec)> & ready_cb) {
    ready_cb_ = ready_cb;
}

void FlvMediaSink::on_flv_tag(const std::function<boost::asio::awaitable<bool>(std::vector<std::shared_ptr<FlvTag>> & msgs)> & cb) {
    cb_ = cb;
}

void FlvMediaSink::close() {
    cb_ = {};
    MediaSink::close();
    return;
}