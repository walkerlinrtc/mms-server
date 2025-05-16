/*
 * @Author: jbl19860422
 * @Date: 2023-07-02 10:56:58
 * @LastEditTime: 2023-12-31 17:36:09
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\hls_live_media_source.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <algorithm>
#include "hls_live_media_source.hpp"
#include "protocol/ts/ts_segment.hpp"

#include "app/publish_app.h"
#include "config/app_config.h"

#include "spdlog/spdlog.h"
using namespace mms;
HlsLiveMediaSource::HlsLiveMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> session, std::shared_ptr<PublishApp> app) : MediaSource("hls", session, app, worker) {

}

HlsLiveMediaSource::~HlsLiveMediaSource() {

}

bool HlsLiveMediaSource::init() {
    return true;
}

bool HlsLiveMediaSource::has_no_sinks_for_time(uint32_t milli_secs) {
    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now_ms - last_sinks_or_bridges_leave_time_ < milli_secs) {
        return false;
    }
    return true;
}

void HlsLiveMediaSource::update_last_access_time() {
    last_sinks_or_bridges_leave_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool HlsLiveMediaSource::on_ts_segment(std::shared_ptr<TsSegment> ts) {
    ts->set_seqno(++curr_seq_no_);
    ts->set_filename(stream_name_ + "/" + std::to_string(curr_seq_no_) + ".ts");
    std::unique_lock<std::shared_mutex> lck(ts_segments_mtx_);
    if (ts_segments_.size() >= 10) {
        ts_segments_.pop_front();
    }
    ts_segments_.push_back(ts);
    update_m3u8();
    return true;
}

std::string HlsLiveMediaSource::get_m3u8() {
    std::shared_lock<std::shared_mutex> lck(m3u8_mtx_);
    return m3u8_;
}

void HlsLiveMediaSource::set_m3u8(const std::string & v) {
    std::unique_lock<std::shared_mutex> lck(m3u8_mtx_);
    m3u8_ = v;
}

void HlsLiveMediaSource::update_m3u8() {
    std::unique_lock<std::shared_mutex> lck(m3u8_mtx_);
    auto app_conf = app_->get_conf();
    if (!app_conf) {
        return;
    }
    
    if (!is_ready_ && ts_segments_.size() >= app_conf->hls_config().min_ts_count_for_m3u8()) {
        is_ready_ = true;
    }

    if (!is_ready_) {
        return;
    }
    
    std::stringstream ss;
    // 获取输出的第一个切片位置
    int first_seg_index = ts_segments_.size() >= 5 ? ts_segments_.size() - 5 : 0;
    // 填写头部
    
    ss << "#EXTM3U\r\n";
    ss << "#EXT-X-VERSION:3\r\n";
    // 获取最大切片时长
    int64_t max_duration = 0;
    for (size_t seg_index = first_seg_index; seg_index < ts_segments_.size(); seg_index++) {
        max_duration = std::max(max_duration, ts_segments_[seg_index]->get_duration());
    }

    ss << "#EXT-X-TARGETDURATION:" << (int)ceil(max_duration / 1000.0) << "\r\n";
    ss << "#EXT-X-MEDIA-SEQUENCE:" << ts_segments_[first_seg_index]->get_seqno() << "\r\n";
    for (size_t seg_index = first_seg_index; seg_index < ts_segments_.size(); seg_index++) {
        max_duration = std::max(max_duration, ts_segments_[seg_index]->get_duration());
    }
    ss.precision(3);
    ss.setf(std::ios::fixed, std::ios::floatfield);
    for (size_t seg_index = first_seg_index; seg_index < ts_segments_.size(); seg_index++) {
        ss << "#EXTINF:" << ts_segments_[seg_index]->get_duration() / 1000.0 << "\r\n";
        ss << ts_segments_[seg_index]->get_filename() << "\r\n";
    }

    m3u8_ = ss.str();
}

std::shared_ptr<TsSegment> HlsLiveMediaSource::get_ts_segment(const std::string & ts_name) {
    std::shared_lock<std::shared_mutex> lck(ts_segments_mtx_);
    for (auto it = ts_segments_.rbegin(); it != ts_segments_.rend(); it++) {
        if ((*it)->get_filename() == ts_name) {
            return *it;
        }
    }
    return nullptr;
}

std::shared_ptr<MediaBridge> HlsLiveMediaSource::get_or_create_bridge(const std::string & id, std::shared_ptr<PublishApp> app, 
                                                                      const std::string & stream_name) {
    ((void)id);
    ((void)app);
    (void)stream_name;
    return nullptr;
}
