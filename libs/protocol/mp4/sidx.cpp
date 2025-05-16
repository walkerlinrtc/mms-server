#include "sidx.h"
#include "base/net_buffer.h"
using namespace mms;


int64_t SegmentIndexEntry::size() {
    return 12;
}

int64_t SegmentIndexEntry::encode(NetBuffer & buf) {
    auto start = buf.pos();
    uint32_t v = uint32_t(reference_type&0x01)<<31;
    v |= referenced_size&0x7fffffff;
    buf.write_4bytes(v);
    buf.write_4bytes(subsegment_duration);
    v = uint32_t(starts_with_SAP&0x01)<<31;
    v |= uint32_t(SAP_type&0x7)<<28;
    v |= SAP_delta_time&0xfffffff;
    buf.write_4bytes(v);
    return buf.pos() - start;
}

int64_t SegmentIndexEntry::decode(NetBuffer & buf) {
    auto start = buf.pos();
    uint32_t v = buf.read_4bytes();

    reference_type = uint8_t((v&0x80000000)>>31);
    referenced_size = v&0x7fffffff;
    subsegment_duration = buf.read_4bytes();
    v = buf.read_4bytes();
    starts_with_SAP = uint8_t((v&0x80000000)>>31);
    SAP_type = uint8_t((v&0x70000000)>>28);
    SAP_delta_time = v&0xfffffff;
    return buf.pos() - start;
}

SidxBox::SidxBox() : FullBox(BOX_TYPE_SIDX, 0, 0) {

}

SidxBox::~SidxBox() {

}

int64_t SidxBox::size() {
    int64_t total_bytes = FullBox::size();
    total_bytes += 8 + (version_==1?16:8) + 4 + entries.size()*12;
    return total_bytes;
}

int64_t SidxBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);
    buf.write_4bytes(reference_id_);
    buf.write_4bytes(timescale_);
    
    if (version_ == 1) {
        buf.write_8bytes(earliest_presentation_time_);
        buf.write_8bytes(first_offset_);
    } else {
        buf.write_4bytes(earliest_presentation_time_);
        buf.write_4bytes(first_offset_);
    }
    buf.write_4bytes(entries.size());

    for (auto & e : entries) {
        e.encode(buf);
    }
    return buf.pos() - start;
}

int64_t SidxBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf); 
    reference_id_ = buf.read_4bytes();
    timescale_ = buf.read_4bytes();
    
    if (version_ == 1) {
        earliest_presentation_time_ = buf.read_8bytes();    
        first_offset_ = buf.read_8bytes();
    } else {
        earliest_presentation_time_ = buf.read_4bytes();
        first_offset_ = buf.read_4bytes();
    }

    uint32_t entry_count = buf.read_4bytes();
    for (size_t i = 0; i < entry_count; i++) {
        SegmentIndexEntry e;
        e.decode(buf);
    }

    return buf.pos() - start;
}