#include "co64.h"
using namespace mms;

Co64Box::Co64Box() : FullBox(BOX_TYPE_CO64, 0, 0) {

}

Co64Box::~Co64Box() {

}

int64_t Co64Box::size() {
    int64_t total_bytes = FullBox::size();
    total_bytes += 4 + entries_.size()*8;
    return total_bytes;
}

int64_t Co64Box::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);
    buf.write_4bytes(entries_.size());
    for (size_t i = 0; i < entries_.size(); i++) {
        buf.write_8bytes(entries_[i]);
    }
    return buf.pos() - start;
}

int64_t Co64Box::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    uint32_t entry_count = buf.read_4bytes();
    if (buf.left_bytes() < entry_count*8) {
        return 0;
    }

    for (size_t i = 0; i < entry_count; i++) {
        entries_[i] = buf.read_8bytes();
    }
    return buf.pos() - start;
}