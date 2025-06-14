/*
 * @Author: jbl19860422
 * @Date: 2023-07-02 10:56:58
 * @LastEditTime: 2023-12-31 17:36:09
 * @LastEditors: jbl19860422
 * @Description:
 * @FilePath: \mms\mms\core\hls_live_media_source.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved.
 */
#include "mpd_live_media_source.hpp"

#include <algorithm>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <chrono>
#include <format>
#include <sstream>

#include "app/publish_app.h"
#include "base/thread/thread_worker.hpp"
#include "base/utils/utils.h"
#include "config/app_config.h"
#include "core/stream_session.hpp"
#include "protocol/mp4/m4s_segment.h"
#include "spdlog/spdlog.h"

using namespace mms;
MpdLiveMediaSource::MpdLiveMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> session,
                                       std::shared_ptr<PublishApp> app)
    : MediaSource("mpd", session, app, worker) {
    availabilityStartTime = Utils::get_utc_time_with_millis();
}

MpdLiveMediaSource::~MpdLiveMediaSource() {}

bool MpdLiveMediaSource::init() { return true; }

bool MpdLiveMediaSource::has_no_sinks_for_time(uint32_t milli_secs) {
    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();
    if (now_ms - last_sinks_or_bridges_leave_time_ < milli_secs) {
        return false;
    }
    return true;
}

void MpdLiveMediaSource::update_last_access_time() {
    last_sinks_or_bridges_leave_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                                            std::chrono::steady_clock::now().time_since_epoch())
                                            .count();
}

bool MpdLiveMediaSource::on_audio_init_segment(std::shared_ptr<Mp4Segment> seg) {
    std::unique_lock<std::shared_mutex> lck(segments_mtx_);
    audio_init_seg_ = seg;
    // update_mpd();
    return true;
}

bool MpdLiveMediaSource::on_video_init_segment(std::shared_ptr<Mp4Segment> seg) {
    std::unique_lock<std::shared_mutex> lck(segments_mtx_);
    video_init_seg_ = seg;
    // update_mpd();
    return true;
}

bool MpdLiveMediaSource::on_audio_segment(std::shared_ptr<Mp4Segment> seg) {
    std::unique_lock<std::shared_mutex> lck(segments_mtx_);
    if (audio_segments_.size() >= 10) {
        audio_segments_.pop_front();
    }
    audio_segments_.push_back(seg);
    update_mpd();
    return true;
}

bool MpdLiveMediaSource::on_video_segment(std::shared_ptr<Mp4Segment> seg) {
    std::unique_lock<std::shared_mutex> lck(segments_mtx_);
    if (video_segments_.size() >= 10) {
        video_segments_.pop_front();
    }
    video_segments_.push_back(seg);
    update_mpd();
    return true;
}

std::string MpdLiveMediaSource::get_mpd() {
    std::shared_lock<std::shared_mutex> lck(mpd_mtx_);
    return mpd_;
}

void MpdLiveMediaSource::set_mpd(const std::string &v) {
    std::unique_lock<std::shared_mutex> lck(mpd_mtx_);
    mpd_ = v;
}

void MpdLiveMediaSource::update_mpd() {
    std::unique_lock<std::shared_mutex> lck(mpd_mtx_);
    auto app_conf = app_->get_conf();
    if (!app_conf) {
        return;
    }

    if (audio_init_seg_ && audio_segments_.size() <= 1) {
        return;
    }

    if (video_init_seg_ && video_segments_.size() <= 1) {
        return;
    }

    double last_duration = 0;
    if (audio_segments_.size() > 0) {
        last_duration = audio_segments_[audio_segments_.size() - 1]->get_duration();
    }

    if (video_segments_.size() > 0) {
        last_duration =
            std::max((double)video_segments_[video_segments_.size() - 1]->get_duration(), last_duration);
    }

    std::stringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>" << std::endl
       << "<MPD "
          "profiles=\"urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash-if-simple\" "
       << std::endl
       << "    ns1:schemaLocation=\"urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd\" " << std::endl
       << "    xmlns=\"urn:mpeg:dash:schema:mpd:2011\" "
          "    xmlns:ns1=\"http://www.w3.org/2001/XMLSchema-instance\" "
       << std::endl
       << "    type=\"dynamic\" " << std::endl
       << "    maxSegmentDuration=\"PT5.0S\"" << std::endl
       << "    minimumUpdatePeriod=\"PT" << 5 << "S\" " << std::endl
       << "    suggestedPresentationDelay=\"PT10S\"" << std::endl
       << "    timeShiftBufferDepth=\"PT" << 6 * (last_duration / 1000) << "S\" " << std::endl
       << "    availabilityStartTime=\"" << availabilityStartTime << "\" " << std::endl
       << "    publishTime=\"" << Utils::get_utc_time_with_millis() << "\" " << std::endl
       << "    minBufferTime=\"PT" << 4 * (last_duration / 1000) << "S\" >" << std::endl;
    ss << "    <BaseURL>" << stream_name_ << "/" << "</BaseURL>" << std::endl;
    ss << "    <Period start=\"PT0S\">" << std::endl;

    if (audio_init_seg_ && !audio_segments_.empty()) {
        size_t start_seg_index = audio_segments_.size() >= 3 ? audio_segments_.size() - 3 : 0;
        ss << "        <AdaptationSet mimeType=\"audio/mp4\" segmentAlignment=\"true\" startWithSAP=\"1\">"
           << std::endl;
        ss << "            <Representation id=\"audio\" bandwidth=\"48000\" codecs=\"mp4a.40.2\">"
           << std::endl;
        ss << "                <SegmentTemplate initialization=\"$RepresentationID$-init.m4s\" "
           << "media=\"$RepresentationID$-$Number$.m4s\" "
           << "startNumber=\"" << audio_segments_.at(start_seg_index)->get_seqno() << "\" "
           << "timescale=\"1000\">" << std::endl;
        ss << "                    <SegmentTimeline>" << std::endl;
        for (size_t i = start_seg_index; i < audio_segments_.size(); ++i) {
            ss << "                        <S t=\"" << audio_segments_.at(i)->get_start_timestamp() << "\" ";
            ss << "d=\"" << audio_segments_.at(i)->get_duration() << "\" />" << std::endl;
        }
        ss << "                    </SegmentTimeline>" << std::endl;
        ss << "                </SegmentTemplate>" << std::endl;
        ss << "            </Representation>" << std::endl;
        ss << "        </AdaptationSet>" << std::endl;
    }

    if (video_init_seg_ && !video_segments_.empty()) {
        size_t start_seg_index = video_segments_.size() >= 3 ? video_segments_.size() - 3 : 0;
        ss << "        <AdaptationSet mimeType=\"video/mp4\" segmentAlignment=\"true\" startWithSAP=\"1\">"
           << std::endl;
        ss << "            <Representation id=\"video\" bandwidth=\"800000\" codecs=\"avc1.64001e\">"
           << std::endl;
        ss << "                <SegmentTemplate initialization=\"$RepresentationID$-init.m4s\" "
           << "media=\"$RepresentationID$-$Number$.m4s\" "
           << "startNumber=\"" << video_segments_.at(start_seg_index)->get_seqno() << "\" "
           << "timescale=\"1000\">" << std::endl;
        ss << "                    <SegmentTimeline>" << std::endl;
        for (size_t i = start_seg_index; i < video_segments_.size(); ++i) {
            ss << "                        <S t=\"" << video_segments_.at(i)->get_start_timestamp() << "\" ";
            ss << "d=\"" << video_segments_.at(i)->get_duration() << "\" />" << std::endl;
        }
        ss << "                    </SegmentTimeline>" << std::endl;
        ss << "                </SegmentTemplate>" << std::endl;
        ss << "            </Representation>" << std::endl;
        ss << "        </AdaptationSet>" << std::endl;
    }
    ss << "    </Period>" << std::endl;
    ss << "</MPD>" << std::endl;
    mpd_ = ss.str();
    // spdlog::info("update mpd:{}", mpd_);
}

std::shared_ptr<Mp4Segment> MpdLiveMediaSource::get_mp4_segment(const std::string &name) {
    std::shared_lock<std::shared_mutex> lck(segments_mtx_);
    if (name == "video-init.m4s") {
        return video_init_seg_;
    } else if (name == "audio-init.m4s") {
        return audio_init_seg_;
    } else if (name.starts_with("audio")) {
        for (auto it = audio_segments_.begin(); it != audio_segments_.end(); it++) {
            if ((*it)->get_filename() == name) {
                return *it;
            }
        }
    } else if (name.starts_with("video")) {
        for (auto it = video_segments_.begin(); it != video_segments_.end(); it++) {
            if ((*it)->get_filename() == name) {
                return *it;
            }
        }
    }
    return nullptr;
}

std::shared_ptr<MediaBridge> MpdLiveMediaSource::get_or_create_bridge(const std::string &id,
                                                                      std::shared_ptr<PublishApp> app,
                                                                      const std::string &stream_name) {
    ((void)id);
    ((void)app);
    ((void)stream_name);
    return nullptr;
}
