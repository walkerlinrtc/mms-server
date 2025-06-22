/*
 * @Author: jbl19860422
 * @Date: 2023-07-02 10:56:58
 * @LastEditTime: 2023-12-27 12:49:11
 * @LastEditors: jbl19860422
 * @Description: 
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "log/log.h"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include "protocol/mp4/m4s_segment.h"
#include "m4s_media_source.hpp"
#include "m4s_media_sink.hpp"

#include "codec/codec.hpp"

#include "base/thread/thread_worker.hpp"
#include "bridge/media_bridge.hpp"
#include "bridge/bridge_factory.hpp"
#include "core/stream_session.hpp"
#include "app/publish_app.h"

using namespace mms;
M4sMediaSource::M4sMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> session, std::shared_ptr<PublishApp> app) : 
                        MediaSource("m4s", session, app, worker) {

}

M4sMediaSource::~M4sMediaSource() {

}

Json::Value M4sMediaSource::to_json() {
    Json::Value v;
    v["type"] = media_type_;
    v["domain"] = domain_name_;
    v["app"] = app_name_;
    v["stream"] = stream_name_;
    v["sinks"] = sinks_count_.load();
    v["create_at"] = create_at_;
    v["stream_time"] = time(NULL) - create_at_;
    v["client_ip"] = client_ip_;
    auto vcodec = video_codec_;
    if (vcodec) {
        v["vcodec"] = vcodec->to_json();
    }

    auto acodec = audio_codec_;
    if (acodec) {
        v["acodec"] = acodec->to_json();
    }
    return v;
}


bool M4sMediaSource::init() {
    return true;
}

bool M4sMediaSource::on_combined_init_segment(std::shared_ptr<Mp4Segment> mp4_seg) {
    combined_init_seg_.store(mp4_seg);
    std::lock_guard<std::recursive_mutex> lck(sinks_mtx_);
    for (auto sink : sinks_) {
        auto s = std::static_pointer_cast<M4sMediaSink>(sink);
        s->recv_combined_init_segment(mp4_seg);
    }
    return true;
}

bool M4sMediaSource::on_audio_init_segment(std::shared_ptr<Mp4Segment> mp4_seg) {
    audio_init_seg_.store(mp4_seg);
    std::lock_guard<std::recursive_mutex> lck(sinks_mtx_);
    for (auto sink : sinks_) {
        auto s = std::static_pointer_cast<M4sMediaSink>(sink);
        s->recv_audio_init_segment(mp4_seg);
    }
    return true;
}

bool M4sMediaSource::on_video_init_segment(std::shared_ptr<Mp4Segment> mp4_seg) {
    video_init_seg_.store(mp4_seg);
    std::lock_guard<std::recursive_mutex> lck(sinks_mtx_);
    for (auto sink : sinks_) {
        auto s = std::static_pointer_cast<M4sMediaSink>(sink);
        s->recv_video_init_segment(mp4_seg);
    }
    return true;
}

bool M4sMediaSource::on_audio_mp4_segment(std::shared_ptr<Mp4Segment> mp4_seg) {
    std::lock_guard<std::recursive_mutex> lck(sinks_mtx_);
    for (auto sink : sinks_) {
        auto s = std::static_pointer_cast<M4sMediaSink>(sink);
        s->recv_audio_mp4_segment(mp4_seg);
    }
    return true;
}

bool M4sMediaSource::on_video_mp4_segment(std::shared_ptr<Mp4Segment> mp4_seg) {
    std::lock_guard<std::recursive_mutex> lck(sinks_mtx_);
    for (auto sink : sinks_) {
        auto s = std::static_pointer_cast<M4sMediaSink>(sink);
        s->recv_video_mp4_segment(mp4_seg);
    }
    return true;
}

bool M4sMediaSource::has_no_sinks_for_time(uint32_t milli_secs) {
    std::lock_guard<std::recursive_mutex> lck1(sinks_mtx_);
    std::shared_lock<std::shared_mutex> lck2(bridges_mtx_);
    if (sinks_.size() > 0 || bridges_.size() > 0) {
        return false;
    }

    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now_ms - last_sinks_or_bridges_leave_time_ < milli_secs) {
        return false;
    }
    return true;
}

std::shared_ptr<MediaBridge> M4sMediaSource::get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, const std::string & stream_name) {
    std::unique_lock<std::shared_mutex> lck(bridges_mtx_);
    std::shared_ptr<MediaBridge> bridge;
    auto it = bridges_.find(id);
    if (it != bridges_.end()) {
        bridge = it->second;
    } 

    if (bridge) {
        return bridge;
    }

    bridge = BridgeFactory::create_bridge(worker_, id, app, std::weak_ptr<MediaSource>(shared_from_this()), app->get_domain_name(), app->get_app_name(), stream_name);
    if (!bridge) {
        return nullptr;
    }
    bridge->init();

    auto media_sink = bridge->get_media_sink();
    media_sink->set_source(shared_from_this());
    MediaSource::add_media_sink(media_sink);

    auto media_source = bridge->get_media_source();
    media_source->set_source_info(app->get_domain_name(), app->get_app_name(), stream_name);
    
    bridges_.insert(std::pair(id, bridge));
    return bridge;
}

bool M4sMediaSource::add_media_sink(std::shared_ptr<MediaSink> media_sink) {
    auto mp4_sink = std::static_pointer_cast<M4sMediaSink>(media_sink);
    auto audio_init_seg = audio_init_seg_.load();
    if (audio_init_seg) {
        mp4_sink->recv_audio_init_segment(audio_init_seg);
    }

    auto video_init_seg = video_init_seg_.load();
    if (video_init_seg) {
        mp4_sink->recv_video_init_segment(video_init_seg);
    }
    
    MediaSource::add_media_sink(media_sink);
    return true;
}