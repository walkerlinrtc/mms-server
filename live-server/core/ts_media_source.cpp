/*
 * @Author: jbl19860422
 * @Date: 2023-07-02 10:56:58
 * @LastEditTime: 2023-12-27 12:49:11
 * @LastEditors: jbl19860422
 * @Description: 
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "log/log.h"
#include "protocol/ts/ts_segment.hpp"
#include "ts_media_source.hpp"
#include "ts_media_sink.hpp"

#include "bridge/media_bridge.hpp"
#include "bridge/bridge_factory.hpp"
#include "recorder/recorder_factory.hpp"

#include "core/stream_session.hpp"
#include "app/publish_app.h"
#include "recorder/recorder.h"
#include "codec/codec.hpp"

using namespace mms;
TsMediaSource::TsMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> session, std::shared_ptr<PublishApp> app) : MediaSource("ts", session, app, worker), pes_pkts_(2048), keyframe_indexes_(200) {

}

TsMediaSource::~TsMediaSource() {

}

std::shared_ptr<Json::Value> TsMediaSource::to_json() {
    std::shared_ptr<Json::Value> d = std::make_shared<Json::Value>();
    Json::Value & v = *d;
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
    return d;
}


bool TsMediaSource::init() {
    return true;
}

bool TsMediaSource::on_ts_segment(std::shared_ptr<TsSegment> ts_seg) {
    std::lock_guard<std::recursive_mutex> lck(sinks_mtx_);
    for (auto sink : sinks_) {
        auto s = std::static_pointer_cast<TsMediaSink>(sink);
        s->recv_ts_segment(ts_seg);
    }
    return true;
}

bool TsMediaSource::has_no_sinks_for_time(uint32_t milli_secs) {
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

bool TsMediaSource::on_pes_packet(std::shared_ptr<PESPacket> pes_packet) {
    if (pes_packet->pes_header.stream_id == TsPESStreamIdVideoCommon) {
        has_video_ = true;
        latest_video_timestamp_ = pes_packet->pes_header.dts/90;
    } else if (pes_packet->pes_header.stream_id == TsPESStreamIdAudioCommon) {
        has_audio_ = true;
    }
    latest_frame_index_ = pes_pkts_.add_pkt(pes_packet);

    if (pes_packet->is_key) {
        std::unique_lock<std::shared_mutex> wlock(keyframe_indexes_rw_mutex_);
        keyframe_indexes_.push_back(latest_frame_index_);
    }
    
    if (latest_frame_index_ <= 300 || latest_frame_index_%10 == 0) {
        std::lock_guard<std::recursive_mutex> lck(sinks_mtx_);
        for (auto sink : sinks_) {
            auto lazy_sink = std::static_pointer_cast<LazyMediaSink>(sink);
            lazy_sink->wakeup();
        }
    }

    stream_ready_ = true;
    return true;
}

std::vector<std::shared_ptr<PESPacket>> TsMediaSource::get_pkts(int64_t &last_pkt_index, uint32_t max_count) {
    std::vector<std::shared_ptr<PESPacket>> pkts;
    if (last_pkt_index == -1) {
        if (!stream_ready_) {
            return {};
        }

        int64_t start_idx = -1;
        if (has_video_) {
            boost::circular_buffer<uint64_t>::reverse_iterator it;
            {
                std::shared_lock<std::shared_mutex> rlock(keyframe_indexes_rw_mutex_);
                it = keyframe_indexes_.rbegin();
                while(it != keyframe_indexes_.rend()) {
                    auto pkt = pes_pkts_.get_pkt(*it);
                    if (pkt) {
                        if (latest_video_timestamp_ - pkt->pes_header.dts/90 >= 2000) {
                            start_idx = *it;
                            break;
                        }
                        it++;
                    } else {
                        start_idx = *it;
                        break;
                    }
                }
            }
        } else {
            int64_t index = (int64_t)pes_pkts_.latest_index();
            while (index >= 0) {
                auto pkt = pes_pkts_.get_pkt(index);
                if (!pkt) {
                    break;
                }

                if (latest_audio_timestamp_ - pkt->pes_header.dts/90 >= 1000) {
                    break;
                }
                index--;
            }
            start_idx = index;
        }

        if (start_idx < 0) {
            pkts.clear();
            return pkts;
        }

        uint32_t pkt_count = 0;
        while(start_idx <= latest_frame_index_ && pkt_count < max_count) {
            auto pkt = pes_pkts_.get_pkt(start_idx);
            if (pkt) {
                pkts.emplace_back(pes_pkts_.get_pkt(start_idx));
                pkt_count++;
            }
            start_idx++;
        }
        last_pkt_index = start_idx;
    } else {
        int64_t start_idx = last_pkt_index;
        uint32_t pkt_count = 0;
        while(start_idx <= latest_frame_index_ && pkt_count < max_count) {
            auto t = pes_pkts_.get_pkt(start_idx);
            if (t) {
                pkts.emplace_back(pes_pkts_.get_pkt(start_idx));
                pkt_count++;
            }
            start_idx++;
        }
        last_pkt_index = start_idx;
    }

    return pkts;
}

std::shared_ptr<MediaBridge> TsMediaSource::get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, const std::string & stream_name) {
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

std::shared_ptr<Recorder> TsMediaSource::get_or_create_recorder(const std::string & record_type, std::shared_ptr<PublishApp> app) {
    CORE_DEBUG("TsMediaSource::get_or_create_recorder");
    std::shared_ptr<Recorder> recorder;
    {
        std::unique_lock<std::shared_mutex> lck(recorder_mtx_);
        auto it = recorders_.find(record_type);
        if (it != recorders_.end()) {
            return it->second;
        }
    }
    
    CORE_INFO("create recorder {}/{}/{}/{}", app_->get_domain_name(), app->get_app_name(), stream_name_, record_type);
    std::shared_ptr<MediaSource> source;
    auto media_type = get_media_type();
    if (record_type != get_media_type()) {
        auto bridge = get_or_create_bridge(media_type + "-" + record_type, app_, stream_name_);
        if (!bridge) {
            return nullptr;
        }

        source = bridge->get_media_source();
    } else {
        source = shared_from_this();
    }

    if (!source) {
        return nullptr;
    }
    
    recorder = RecorderFactory::create_recorder(worker_, record_type, app, std::weak_ptr<MediaSource>(source), app->get_domain_name(), app->get_app_name(), stream_name_);
    if (!recorder->init()) {
        return nullptr;
    }
    std::unique_lock<std::shared_mutex> lck(recorder_mtx_);
    recorders_.insert(std::pair(record_type, recorder));
    return recorder;
}