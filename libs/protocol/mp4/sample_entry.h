#pragma once
#include <stdint.h>
#include "box.h"

namespace mms {
class NetBuffer;
class SampleEntry : public Box {
public:
    SampleEntry(Box::Type type);
    virtual ~SampleEntry();
public:
    virtual int64_t size();
    virtual int64_t encode(NetBuffer & buf);
    virtual int64_t decode(NetBuffer & buf);
public:
    uint8_t reserved_[6];
    uint16_t data_reference_index_;
};
} // namespace mms
