#pragma once
#include "full_box.h"
namespace mms {
// 8.8.3 Track Extends Box(trex)
// ISO_IEC_14496-12-base-format-2012.pdf, page 65
class TrexBox : public FullBox {
public:
    TrexBox();
    virtual ~TrexBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    // identifies the track; this shall be the track ID of a track in the Movie Box
    uint32_t track_ID_;
    // These fields set up defaults used in the track fragments.
    uint32_t default_sample_description_index_;
    uint32_t default_sample_duration_ = 0;
    uint32_t default_sample_size_ = 0;
    uint32_t default_sample_flags_ = 0;
};
};