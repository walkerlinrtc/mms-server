#include "stts.h"
#include "base/net_buffer.h"
using namespace mms;

int64_t SttsEntry::size() {
    return 8;
}

int64_t SttsEntry::encode(NetBuffer & buf) {
    auto start = buf.pos();
    buf.write_4bytes(sample_count);
    buf.write_4bytes(sample_delta);
    return buf.pos() - start;
}

int64_t SttsEntry::decode(NetBuffer & buf) {
    auto start = buf.pos();
    sample_count = buf.read_4bytes();
    sample_delta = buf.read_4bytes();
    return buf.pos() - start;
}


SttsBox::SttsBox() : FullBox(BOX_TYPE_STTS, 0, 0) {

}

SttsBox::~SttsBox() {

}

int64_t SttsBox::size() {
    int64_t total_bytes = FullBox::size() + 4 + entries_.size()*8;
    return total_bytes;
}

int64_t SttsBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);
    buf.write_4bytes(entries_.size());
    for (auto & e : entries_) {
        e.encode(buf);
    }
    return buf.pos() - start;
}

int64_t SttsBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    uint32_t entry_count = buf.read_4bytes();
    for (size_t i = 0; i < entry_count; i++) {
        SttsEntry e;
        e.decode(buf);
        entries_.push_back(e);
    }
    return buf.pos() - start;
}