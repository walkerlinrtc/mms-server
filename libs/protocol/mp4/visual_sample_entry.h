#pragma once
#include <stdint.h>
#include "sample_entry.h"

namespace mms {
class NetBuffer;
class VisualSampleEntry : public SampleEntry {
public:
    VisualSampleEntry(Box::Type type);
    virtual ~VisualSampleEntry();
public:
    virtual int64_t size();
    virtual int64_t encode(NetBuffer & data);
    virtual int64_t decode(NetBuffer & data);
public:
    void set_avcc_box(std::shared_ptr<Box> avcc);
    void set_hvcc_box(std::shared_ptr<Box> hvcc);
public:
    uint16_t pre_defined1_ = 0; 
    uint16_t reserved1_ = 0; 
    uint32_t pre_defined2_[3] = {0}; 
    uint16_t width_; 
    uint16_t height_; 
    uint32_t horizresolution_ = 0x00480000; // 72 dpi 
    uint32_t vertresolution_ = 0x00480000; // 72 dpi 
    uint32_t reserved2_ = 0; 
    uint16_t frame_count_ = 1; 
    char compressorname_[32]; 
    uint16_t depth_ = 0x0018; 
    int16_t  pre_defined3_ = -1;

    std::shared_ptr<Box> avcc_;
    std::shared_ptr<Box> hvcc_;
};
} // namespace mms
