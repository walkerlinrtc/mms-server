#pragma once
#include <stdint.h>
#include "sample_entry.h"

namespace mms {
class NetBuffer;
class EsdsBox;
class AudioSampleEntry : public SampleEntry {
public:
    AudioSampleEntry(Box::Type type);
    virtual ~AudioSampleEntry();
public:
    virtual int64_t size();
    virtual int64_t encode(NetBuffer & buf);
    virtual int64_t decode(NetBuffer & buf);
public:
    uint32_t reserved1_[2] = {0}; 
    uint16_t channel_count_ = 2; 
    uint16_t sample_size_ = 16; 
    uint16_t pre_defined_ = 0;
    uint16_t reserved2_;
    uint32_t sample_rate_ = 0;
    std::shared_ptr<EsdsBox> esds_;
};
} // namespace mms
