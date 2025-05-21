#pragma once
#include <string>
#include <string_view>
#include <memory>
#include <boost/asio/buffer.hpp>

#include "base/utils/utils.h"
namespace mms {
class TsSegment {
public:
    TsSegment();
    virtual ~TsSegment();

    const std::string & get_filename();

    void set_filename(const std::string & filename);

    void set_seqno(int64_t v);

    int64_t get_seqno();

    int64_t get_audio_start_pts();
    int64_t get_audio_end_pts();

    void update_audio_pts(int64_t pts);
    void update_video_dts(int64_t dts);
    int64_t get_start_pts();

    bool has_video() {
        return start_dts_video_ != -1;
    }

    bool has_audio() {
        return start_pts_audio_ != -1;
    }

    int64_t get_create_at() {
        return create_at_;
    }
    
    int64_t get_duration();

    int32_t get_curr_ts_offset();
    int32_t get_curr_ts_index();
    std::string_view alloc_ts_packet();
    std::vector<std::string_view> get_ts_data();
    std::vector<boost::asio::const_buffer> get_ts_seg(size_t ts_index, size_t ts_off, int32_t ts_bytes);
    int64_t get_ts_bytes();
    void set_reaped() {
        is_reaped_ = true;
    }

    inline bool is_reaped() {
        return is_reaped_;
    }
private:
    int64_t create_at_ = Utils::get_current_ms();
    int64_t start_dts_video_ = -1;
    int64_t end_dts_video_ = 0;
    int64_t start_pts_audio_ = -1;
    int64_t end_pts_audio_ = 0;

    int64_t duration_video_ms_ = 0;
    int64_t duration_audio_ms_ = 0;

    std::string filename_;
    int64_t seq_ = 0;

    std::vector<std::unique_ptr<uint8_t[]>> ts_bufs_;
    int32_t ts_buf_index_ = 0;
    size_t ts_buf_len_ = 0;
    int64_t total_ts_bytes_ = 0;
    bool is_reaped_  = false;

    constexpr static int SINGLE_TS_BYTES = 188*4096; 
};
};