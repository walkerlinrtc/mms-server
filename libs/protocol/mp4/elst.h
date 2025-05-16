#pragma once
#include <vector>
#include <memory>

#include "full_box.h"
namespace mms {
class NetBuffer;
class ElstEntry {
public:
    ElstEntry() {};
    virtual ~ElstEntry() {};
    int64_t size(uint8_t version);
    int64_t encode(NetBuffer & buf, uint8_t version);
    int64_t decode(NetBuffer & buf, uint8_t version);
public:
    // An integer that specifies the duration of this edit segment in units of the timescale
    // in the Movie Header Box
    uint64_t segment_duration_;
    // An integer containing the starting time within the media of this edit segment (in media time
    // scale units, in composition time). If this field is set to –1, it is an empty edit. The last edit in a track
    // shall never be an empty edit. Any difference between the duration in the Movie Header Box, and the
    // track’s duration is expressed as an implicit empty edit at the end.
    int64_t media_time_;
public:
    // specifies the relative rate at which to play the media corresponding to this edit segment. If this value is 0,
    // Then the edit is specifying a ‘dwell’: the media at media-time is presented for the segment-duration. Otherwise
    // this field shall contain the value 1.
    int16_t media_rate_integer_;
    int16_t media_rate_fraction_;
};

class ElstBox : public FullBox {
public:
    ElstBox();
    virtual ~ElstBox();
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
protected:
    std::vector<ElstEntry> entries_;
};
};