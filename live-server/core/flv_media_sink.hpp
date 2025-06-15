/*
 * @Author: jbl19860422
 * @Date: 2023-09-17 16:03:11
 * @LastEditTime: 2023-09-17 16:03:23
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\flv_media_sink.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <vector>
#include <set>
#include <memory>
#include <atomic>
#include <functional>

#include <boost/asio/awaitable.hpp>

#include "media_sink.hpp"
#include "base/obj_tracker.hpp"

namespace mms {
class Codec;
class ThreadWorker;
class FlvTag;

class FlvMediaSink : public LazyMediaSink, public ObjTracker<FlvMediaSink> {
public:
    FlvMediaSink(ThreadWorker *worker);
    virtual ~FlvMediaSink();
    virtual bool init();
    virtual bool on_audio_packet(std::shared_ptr<FlvTag> audio_pkt);
    virtual bool on_video_packet(std::shared_ptr<FlvTag> video_pkt);
    virtual boost::asio::awaitable<bool> send_flv_tag(std::shared_ptr<FlvTag> pkt);
    bool on_metadata(std::shared_ptr<FlvTag> metadata_pkt);
    virtual bool on_source_codec_ready(std::shared_ptr<Codec> video_codec, std::shared_ptr<Codec> audio_codec);
    void set_source_codec_ready_cb(const std::function<bool(std::shared_ptr<Codec> video_codec, std::shared_ptr<Codec> audio_codec)> & ready_cb);
    boost::asio::awaitable<void> do_work() override;
    void on_flv_tag(const std::function<boost::asio::awaitable<bool>(std::vector<std::shared_ptr<FlvTag>> & msgs)> & cb);
    void close() override;
protected:
    int64_t last_send_pkt_index_ = -1;
    bool has_video_; 
    bool has_audio_;
    bool stream_ready_;
    std::function<boost::asio::awaitable<bool>(std::vector<std::shared_ptr<FlvTag>> & msgs)> cb_;
    std::function<bool(std::shared_ptr<Codec> video_codec, std::shared_ptr<Codec> audio_codec)> ready_cb_;
};
};