#pragma once
#include "full_box.h"
namespace mms {
class NetBuffer;
class StcoBox : public FullBox {
public:
    StcoBox();
    virtual ~StcoBox();

    int64_t size() override;
    int64_t encode(NetBuffer& data) override;
    int64_t decode(NetBuffer& data) override;
public:
    // A 32 bit integer that gives the offset of the start of a chunk into its containing
    // media file.
    std::vector<uint32_t> entries;
};
};