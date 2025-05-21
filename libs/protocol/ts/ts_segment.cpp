#include <string>
#include <string_view>
#include <memory>
#include "string.h"

#include "ts_segment.hpp"
#include "spdlog/spdlog.h"
using namespace mms;

TsSegment::TsSegment() {
    ts_buf_ = std::make_unique<uint8_t[]>(188*4096);
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

std::string_view TsSegment::alloc_ts_packet() {
    if ((ts_buf_len_ + 188) > ts_buf_max_len_) {
        auto ts_buf_new = std::make_unique<uint8_t[]>(ts_buf_max_len_*2);
        ts_buf_max_len_ = ts_buf_max_len_*2;
        memcpy(ts_buf_new.get(), ts_buf_.get(), ts_buf_len_);
        ts_buf_.swap(ts_buf_new);
    }

    auto pkt = std::string_view((char*)ts_buf_.get() + ts_buf_len_, 188);
    ts_buf_len_ += 188;
    return pkt;
}

std::string_view TsSegment::get_ts_data() {
    return std::string_view((char*)ts_buf_.get(), ts_buf_len_);
}

int64_t TsSegment::get_ts_bytes() {
    return ts_buf_len_;
}