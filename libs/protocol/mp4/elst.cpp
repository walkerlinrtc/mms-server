#include <string_view>
#include "elst.h"

using namespace mms;

int64_t ElstEntry::size(uint8_t version) {
    if (version == 1) {
        return 20;
    } else {
        return 12;
    }
}

int64_t ElstEntry::encode(NetBuffer & buf, uint8_t version) {
    auto start = buf.pos();
    if (version == 1) {
        segment_duration_ = buf.read_8bytes();
        media_time_ = buf.read_8bytes();
    } else {
        segment_duration_ = buf.read_4bytes();
        media_time_ = buf.read_4bytes();
    }
    media_rate_integer_ = buf.read_2bytes();
    media_rate_fraction_ = buf.read_2bytes();
    return buf.pos() - start;
}

int64_t ElstEntry::decode(NetBuffer & buf, uint8_t version) {
    auto start = buf.pos();
    if (version == 1) {
        segment_duration_ = buf.read_8bytes();
        media_time_ = buf.read_8bytes();
    } else {
        segment_duration_ = buf.read_4bytes();
        media_time_ = buf.read_4bytes();
    }
    media_rate_integer_ = buf.read_2bytes();
    media_rate_fraction_ = buf.read_2bytes();
    return buf.pos() - start;
}

ElstBox::ElstBox() : FullBox(BOX_TYPE_ELST, 0, 0) {

}

ElstBox::~ElstBox() {

}

int64_t ElstBox::size() {
    int64_t total_bytes = FullBox::size();
    for (auto & e : entries_) {
        total_bytes += e.size(version_);
    }
    return total_bytes;
}

int64_t ElstBox::encode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::encode(buf);

    for (auto & e : entries_) {
        e.encode(buf, version_);
    }

    return buf.pos() - start;
}

int64_t ElstBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    int64_t left_bytes = decoded_size() - (buf.pos() - start);
    while (left_bytes > 0) {
        ElstEntry ee;
        auto consumed = ee.decode(buf, version_);
        entries_.push_back(ee);
        left_bytes -= consumed;
    }
    return buf.pos() - start;
}