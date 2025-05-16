#pragma once 
#include <vector>
#include "full_box.h"
namespace mms {
class NetBuffer;
// Entry for trun.
// ISO_IEC_14496-12-base-format-2012.pdf, page 69

// The tr_flags for trun
// ISO_IEC_14496-12-base-format-2012.pdf, page 69
enum TrunFlags
{
    // data-offset-present.
    TrunFlagsDataOffset = 0x000001,
    // this over-rides the default flags for the first sample only. This
    // makes it possible to record a group of frames where the first is a key and the rest are difference
    // frames, without supplying explicit flags for every sample. If this flag and field are used, sample-flags
    // shall not be present.
    TrunFlagsFirstSample = 0x000004,
    // indicates that each sample has its own duration, otherwise the default is used.
    TrunFlagsSampleDuration = 0x000100,
    // Each sample has its own size, otherwise the default is used.
    TrunFlagsSampleSize = 0x000200,
    // Each sample has its own flags, otherwise the default is used.
    TrunFlagsSampleFlag = 0x000400,
    // Each sample has a composition time offset (e.g. as used for I/P/B video in MPEG).
    TrunFlagsSampleCtsOffset = 0x000800,
};

class TrunEntry {
public:
    TrunEntry(FullBox* o);
    virtual ~TrunEntry();
public:
    int64_t size();
    int64_t encode(NetBuffer & buf);
    int64_t decode(NetBuffer & buf);
public:
    FullBox* owner_;
    
    uint32_t sample_duration_;
    uint32_t sample_size_;
    uint32_t sample_flags_;
    // if version == 0, unsigned int(32); otherwise, signed int(32).
    int64_t sample_composition_time_offset_;
};

class TrunBox : public FullBox {
public:
    TrunBox(uint32_t flags);
    virtual ~TrunBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    std::vector<TrunEntry> entries_;
public:
    // added to the implicit or explicit data_offset established in the track fragment header.
    int32_t data_offset_;
    // provides a set of flags for the first sample only of this run.
    uint32_t first_sample_flags_;
};
};