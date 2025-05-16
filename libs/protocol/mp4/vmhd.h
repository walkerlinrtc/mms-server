#pragma once
#include "full_box.h"
namespace mms {
class NetBuffer;
class VmhdBox : public FullBox {
public:
public:
    VmhdBox();
    virtual ~VmhdBox();
public:
    virtual int64_t size();
    virtual int64_t encode(NetBuffer & buf);
    virtual int64_t decode(NetBuffer & buf);
public:
    // A composition mode for this video track, from the following enumerated set,
    // which may be extended by derived specifications:
    //      copy = 0 copy over the existing image
    uint16_t graphicsmode_ = 0;
    // A set of 3 colour values (red, green, blue) available for use by graphics modes
    uint16_t opcolor_[3] = {0};
};

};