#include "stss.h"

using namespace mms;

StssBox::StssBox() : FullBox(BOX_TYPE_STSS, 0, 0) {

}

StssBox::~StssBox() {

}

int64_t StssBox::size() {
    int64_t total_bytes = FullBox::size() + 4 + sample_entries_.size()*4;
    return total_bytes;
}

int64_t StssBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);
    buf.write_4bytes(sample_entries_.size());
    for (size_t i = 0; i < sample_entries_.size(); i++) {
        buf.write_4bytes(sample_entries_[i]);
    }

    return buf.pos() - start;
}

int64_t StssBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    uint32_t sample_count = buf.read_4bytes();
    for (size_t i = 0; i < sample_count; i++) {
        sample_entries_.push_back(buf.read_4bytes());
    }
    
    return buf.pos() - start;
}

bool StssBox::is_sync(uint32_t sample_index) {
    (void)sample_index;
    return true;
}