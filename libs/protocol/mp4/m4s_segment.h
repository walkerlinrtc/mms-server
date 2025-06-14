#pragma once
#include <string>
#include <string_view>
#include <memory>

#include "base/utils/utils.h"

namespace mms {
class Mp4Segment {
public:
    static constexpr size_t MP4_DEFAULT_SIZE = 1 * 1024 * 1024;
    Mp4Segment();
    virtual ~Mp4Segment() {

    }
public:
    const std::string & get_filename() {
        return filename_;
    }

    void set_filename(const std::string & filename) {
        filename_ = filename;
    }

    void set_reaped() {
        is_reaped_ = true;
    }

    inline bool is_reaped() {
        return is_reaped_;
    }

    std::string_view alloc_buffer(size_t s);
    std::string_view get_used_buf() {
        return std::string_view((char*)buf_.get(), used_bytes_);
    }


    void update_timestamp(int64_t start, int64_t end);

    int64_t get_start_timestamp() {
        return start_timestamp_;
    }

    int64_t get_duration();
    void set_seqno(int64_t v) {
        seq_no_ = v;
    }

    int64_t get_seqno() {
        return seq_no_;
    }

    int64_t get_create_at() {
        return create_at_;
    }

    int64_t get_bytes() {
        return used_bytes_;
    }
protected:
    std::string filename_;
    uint32_t seq_no_ = 0;

    int64_t create_at_ = Utils::get_current_ms();
    int64_t start_timestamp_ = -1;
    int64_t end_timestamp_ = 0;
    int64_t duration_ms_ = 0;

    std::unique_ptr<uint8_t[]> buf_;
    size_t used_bytes_ = 0;
    size_t allocated_bytes_;

    bool is_reaped_  = false;
};
};