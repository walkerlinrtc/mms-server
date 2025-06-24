#pragma once
#include "full_box.h"
namespace mms {
class NetBuffer;
// The entry for SegmentIndexBox(sidx) for MPEG-DASH.
// @doc https://patches.videolan.org/patch/103/
struct SegmentIndexEntry {
    uint8_t reference_type; // 1bit
    uint32_t referenced_size; // 31bits
    uint32_t subsegment_duration; // 32bits
    uint8_t starts_with_SAP; // 1bit
    uint8_t SAP_type; // 3bits
    uint32_t SAP_delta_time; // 28bits
public:
    int64_t size();
    int64_t encode(NetBuffer & buf);
    int64_t decode(NetBuffer & buf);
};

// The SegmentIndexBox(sidx) for MPEG-DASH.
// @doc https://gpac.wp.imt.fr/2012/02/01/dash-support/
// @doc https://patches.videolan.org/patch/103/
// @doc https://github.com/necccc/iso-bmff-parser-stream/blob/master/lib/box/sidx.js
class SidxBox : public FullBox {
public:
    SidxBox();
    virtual ~SidxBox();
public:
    uint32_t reference_id_ = 0;
    uint32_t timescale_ = 0;
    uint64_t earliest_presentation_time_ = 0;
    uint64_t first_offset_ = 0;
    std::vector<SegmentIndexEntry> entries;
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
};
};