#include <string.h>

#include "m4s_segment.h"
using namespace mms;

Mp4Segment::Mp4Segment() {
    buf_ = std::make_unique<uint8_t[]>(MP4_DEFAULT_SIZE);
    used_bytes_ = 0;
    allocated_bytes_ = MP4_DEFAULT_SIZE;
}

std::string_view Mp4Segment::alloc_buffer(size_t s) {
    if ((used_bytes_ + s) >= allocated_bytes_) {
        auto buf_new = std::make_unique<uint8_t[]>(allocated_bytes_*2);
        allocated_bytes_ = allocated_bytes_*2;
        memcpy(buf_new.get(), buf_.get(), used_bytes_);
        buf_.swap(buf_new);
    }

    auto pkt = std::string_view((char*)buf_.get() + used_bytes_, s);
    used_bytes_ += s;
    return pkt;
}

void Mp4Segment::update_timestamp(int64_t start, int64_t end) {
    static const int64_t max_ms = 0x20c49ba5e353f7LL;
    if (start < 0 || start >= max_ms) {
        start = 0;
    }

    if (end < 0 || end >= max_ms) {
        end = 0;
    }


    start_timestamp_ = start;
    end_timestamp_ = end;
    
    duration_ms_ = end - start;
}

int64_t Mp4Segment::get_duration() {
    return duration_ms_;
}