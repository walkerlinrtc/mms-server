#include "trex.h"
#include "base/net_buffer.h"
using namespace mms;

TrexBox::TrexBox() : FullBox(BOX_TYPE_TREX, 0, 0) {

}

TrexBox::~TrexBox() {

}

int64_t TrexBox::size() {
    int64_t total_bytes = FullBox::size();
    total_bytes += 4*5;
    return total_bytes;
}

int64_t TrexBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);
    buf.write_4bytes(track_ID_);
    buf.write_4bytes(default_sample_description_index_);
    buf.write_4bytes(default_sample_duration_);
    buf.write_4bytes(default_sample_size_);
    buf.write_4bytes(default_sample_flags_);
    return buf.pos() - start;
}

int64_t TrexBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    track_ID_ = buf.read_4bytes();
    default_sample_description_index_ = buf.read_4bytes();
    default_sample_duration_ = buf.read_4bytes();
    default_sample_size_ = buf.read_4bytes();
    default_sample_flags_ = buf.read_4bytes();
    return buf.pos() - start;
}

