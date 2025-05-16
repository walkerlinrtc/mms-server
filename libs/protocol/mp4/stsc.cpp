#include "stsc.h"
#include "mp4_factory.h"

#include <string.h>
using namespace mms;

int64_t StscEntry::encode(NetBuffer & buf) {
    auto start = buf.pos();
    buf.write_4bytes(first_chunk);
    buf.write_4bytes(samples_per_chunk);
    buf.write_4bytes(sample_description_index);
    return buf.pos() - start;
}

int64_t StscEntry::decode(NetBuffer & buf) {
    auto start = buf.pos();
    first_chunk = buf.read_4bytes();
    samples_per_chunk = buf.read_4bytes();
    sample_description_index = buf.read_4bytes();
    return buf.pos() - start;
}

StscBox::StscBox() : FullBox(BOX_TYPE_STSC, 0, 0) {

}

StscBox::~StscBox() {

}

int64_t StscBox::size() {
    int64_t total_bytes = FullBox::size();
    total_bytes += 4 + entries_.size()*12;
    return total_bytes;
}

int64_t StscBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);
    buf.write_4bytes(entries_.size());
    for (size_t i = 0; i < entries_.size(); i++) {
        entries_[i].encode(buf);
    }

    return buf.pos() - start;
}

int64_t StscBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);

    size_t entry_count = buf.read_4bytes();
    for (size_t i = 0; i < entry_count; i++) {
        StscEntry se;
        se.decode(buf);
        entries_.push_back(se);
    }
    
    return buf.pos() - start;
}