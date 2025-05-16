#include "stsz.h"
#include "mp4_factory.h"

#include <string.h>
using namespace mms;

StszBox::StszBox() : FullBox(BOX_TYPE_STSZ, 0, 0) {

}

StszBox::~StszBox() {

}

int64_t StszBox::size() {
    int64_t total_bytes = FullBox::size();
    total_bytes += 4 + 4;
    if (sample_size_ == 0) {
        total_bytes += 4*entry_sizes_.size();
    }
    return total_bytes;
}

int64_t StszBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);
    buf.write_4bytes(sample_size_);
    buf.write_4bytes(entry_sizes_.size());
    for (size_t i = 0; i < entry_sizes_.size() && sample_size_ == 0; i++) {
        buf.write_4bytes(entry_sizes_[i]);
    }
    return buf.pos() - start;
}

int64_t StszBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    sample_size_ = buf.read_4bytes();
    size_t entry_count = buf.read_4bytes();
    for (size_t i = 0; i < entry_count; i++) {
        entry_sizes_.push_back(buf.read_4bytes());
    }
    
    return buf.pos() - start;
}