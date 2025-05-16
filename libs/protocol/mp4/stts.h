#pragma once
#include "full_box.h"
namespace mms {
// 8.6.1.2 Decoding Time to Sample Box (stts), for Audio/Video.
// ISO_IEC_14496-12-base-format-2012.pdf, page 48
class SttsEntry {
public:
    // An integer that counts the number of consecutive samples that have the given
    // duration.
    uint32_t sample_count;
    // An integer that gives the delta of these samples in the time-scale of the media.
    uint32_t sample_delta;
public:
    int64_t size();
    int64_t encode(NetBuffer & buf);
    int64_t decode(NetBuffer & buf);
};

class SttsBox : public FullBox {
public:
    SttsBox();
    virtual ~SttsBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    std::vector<SttsEntry> entries_;
};
};