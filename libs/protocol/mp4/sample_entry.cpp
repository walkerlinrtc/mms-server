#include "sample_entry.h"
#include "base/net_buffer.h"
using namespace mms;

SampleEntry::SampleEntry(Box::Type type) : Box(type) {
    
}

SampleEntry::~SampleEntry() {

}

int64_t SampleEntry::size() {
    int64_t total_bytes = Box::size();
    total_bytes += 8;
    return total_bytes;
}

int64_t SampleEntry::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    Box::encode(buf);
    buf.skip(6);
    buf.write_2bytes(data_reference_index_);
    return buf.pos() - start;
}

int64_t SampleEntry::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Box::decode(buf);
    buf.skip(6);
    data_reference_index_ = buf.read_2bytes();
    return buf.pos() - start;
}