#pragma once
#include "full_box.h"
namespace mms {
class NetBuffer;
// 8.8.12 Track fragment decode time (tfdt)
// ISO_IEC_14496-12-base-format-2012.pdf, page 72
// The Track Fragment Base Media Decode Time Box provides the absolute decode time, measured on
// The media timeline, of the first sample in decode order in the track fragment. This can be useful, for example,
// when performing random access in a file; it is not necessary to sum the sample durations of all preceding
// samples in previous fragments to find this value (where the sample durations are the deltas in the Decoding
// Time to Sample Box and the sample_durations in the preceding track runs).
class TfdtBox : public FullBox {
public:
    TfdtBox();
    virtual ~TfdtBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    uint64_t base_media_decode_time;
};
};