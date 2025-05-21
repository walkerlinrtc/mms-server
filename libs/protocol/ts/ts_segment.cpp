#include <string>
#include <string_view>
#include <memory>
#include "string.h"

#include "ts_segment.hpp"
#include "spdlog/spdlog.h"
using namespace mms;

TsSegment::TsSegment() {
    ts_bufs_.emplace_back(std::move(std::make_unique<uint8_t[]>(188*4096)));
    ts_buf_index_ = 0;
    ts_buf_max_len_ = 188*4096;
}

TsSegment::~TsSegment() {

}

const std::string & TsSegment::get_filename() {
    return filename_;
}

void TsSegment::set_filename(const std::string & filename) {
    filename_ = filename;
}

void TsSegment::set_seqno(int64_t v) {
    seq_ = v;
}

int64_t TsSegment::get_seqno() {
    return seq_;
}

int64_t TsSegment::get_audio_start_pts() {
    return start_pts_audio_;
}

int64_t TsSegment::get_audio_end_pts() {
    return end_pts_audio_;
}

void TsSegment::update_audio_pts(int64_t pts) {
    static const int64_t max_ms = 0x20c49ba5e353f7LL;
    if (pts < 0 || pts >= max_ms) {
        pts = 0;
    }

    if (start_pts_audio_ == -1) {
        start_pts_audio_ = pts;
    }

    start_pts_audio_ = std::min(pts, start_pts_audio_);
    if (pts > end_pts_audio_) {
        end_pts_audio_ = pts;
    }
    
    duration_audio_ms_ = end_pts_audio_ - start_pts_audio_;
}

void TsSegment::update_video_dts(int64_t dts) {
    static const int64_t max_ms = 0x20c49ba5e353f7LL;
    if (dts < 0 || dts >= max_ms) {
        dts = 0;
    }

    if (start_dts_video_ == -1) {
        start_dts_video_ = dts;
    }

    start_dts_video_ = std::min(dts, start_dts_video_);
    if (dts > end_dts_video_) {
        end_dts_video_ = dts;
    }
    
    duration_video_ms_ = end_dts_video_ - start_dts_video_;
}

int64_t TsSegment::get_start_pts() {
    return std::min(start_dts_video_, start_pts_audio_);
}

int64_t TsSegment::get_duration() {
    if (has_video() && has_audio()) {
        return std::min(duration_video_ms_, duration_audio_ms_);
    } else if (has_video()) {
        return duration_video_ms_;
    } else if (has_audio()) {
        return duration_audio_ms_;
    }
    return 0;
}

int32_t TsSegment::get_curr_ts_offset() {
    return ts_buf_len_;
}

int32_t TsSegment::get_curr_ts_index() {
    return ts_buf_index_;
}

std::string_view TsSegment::alloc_ts_packet() {
    if (ts_buf_len_ >= ts_buf_max_len_) {
        auto new_ts_buf = std::make_unique<uint8_t[]>(ts_buf_max_len_);
        ts_bufs_.push_back(std::move(new_ts_buf));
        ts_buf_index_++;
        ts_buf_len_ = 0;
    }

    auto pkt = std::string_view((char*)ts_bufs_[ts_buf_index_].get() + ts_buf_len_, 188);
    ts_buf_len_ += 188;
    total_ts_bytes_ += 188;
    return pkt;
}

std::vector<std::string_view> TsSegment::get_ts_data() {
    std::vector<std::string_view> vs;
    for (auto it = ts_bufs_.begin(); it != ts_bufs_.end(); it++) {
        if ((it + 1) == ts_bufs_.end()) {
            vs.push_back(std::string_view((char*)(*it).get(), ts_buf_len_));
        } else {
            vs.push_back(std::string_view((char*)(*it).get(), ts_buf_max_len_));
        }
    }

    return vs;
}

int64_t TsSegment::get_ts_bytes() {
    return total_ts_bytes_;
}