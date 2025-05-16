#pragma once
#include "full_box.h"
namespace mms {
class NetBuffer;
class Co64Box : public FullBox {
public:
    Co64Box();
    virtual ~Co64Box();

    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    // A 64 bit integer that gives the offset of the start of a chunk into its containing
    // media file.
    std::vector<uint64_t> entries_;
};
};