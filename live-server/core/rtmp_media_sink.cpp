/*
 * @Author: jbl19860422
 * @Date: 2023-07-02 10:56:58
 * @LastEditTime: 2023-12-27 12:49:11
 * @LastEditors: jbl19860422
 * @Description: 
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include "rtmp_media_sink.hpp"
#include "rtmp_media_source.hpp"
#include "log/log.h"

using namespace mms;

RtmpMediaSink::RtmpMediaSink(ThreadWorker *worker) : LazyMediaSink(worker) {
    CORE_DEBUG("create RtmpMediaSink");
    pkts_.reserve(20);
}

bool RtmpMediaSink::init() {
    return true;
}

RtmpMediaSink::~RtmpMediaSink() {
    CORE_DEBUG("destroy RtmpMediaSink");
}

bool RtmpMediaSink::on_audio_packet(std::shared_ptr<RtmpMessage> audio_pkt) {
    auto self(this->shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self, audio_pkt]()->boost::asio::awaitable<void> {
        std::vector<std::shared_ptr<RtmpMessage>> v = {audio_pkt};
        co_await rtmp_msg_cb_(v);
        co_return;
    }, boost::asio::detached);
    return true;
}

bool RtmpMediaSink::on_video_packet(std::shared_ptr<RtmpMessage> video_pkt) {
    auto self(this->shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self, video_pkt]()->boost::asio::awaitable<void> {
        std::vector<std::shared_ptr<RtmpMessage>> v = {video_pkt};
        co_await rtmp_msg_cb_(v);
        co_return;
    }, boost::asio::detached);
    return true;
}

bool RtmpMediaSink::on_metadata(std::shared_ptr<RtmpMessage> metadata_pkt) {
    auto self(this->shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self, metadata_pkt]()->boost::asio::awaitable<void> {
        std::vector<std::shared_ptr<RtmpMessage>> v = {metadata_pkt};
        co_await rtmp_msg_cb_(v);
        co_return;
    }, boost::asio::detached);
    return true;
} 

boost::asio::awaitable<void> RtmpMediaSink::do_work() {
    if (source_ == nullptr || !source_->is_stream_ready()) {
        co_return;
    }

    if (!rtmp_msg_cb_) {
        co_return;
    }

    auto rtmp_source = std::static_pointer_cast<RtmpMediaSource>(source_);
    do {
        pkts_ = rtmp_source->get_pkts(last_send_pkt_index_, 20);
        if (pkts_.size() <= 0) {
            break;
        }
        
        if (!co_await rtmp_msg_cb_(pkts_)) {
            close();
            break;
        }
    } while (pkts_.size() > 0);

    co_return;
}

void RtmpMediaSink::on_rtmp_message(const std::function<boost::asio::awaitable<bool>(const std::vector<std::shared_ptr<RtmpMessage>> & msgs)> & cb) {
    rtmp_msg_cb_ = cb;
}

void RtmpMediaSink::close() {
    rtmp_msg_cb_ = {};
    LazyMediaSink::close();
}