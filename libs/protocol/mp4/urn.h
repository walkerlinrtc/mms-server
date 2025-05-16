#pragma once
#include "full_box.h"
namespace mms {
class NetBuffer;
// 8.7.2 Data Reference Box (urn )
// ISO_IEC_14496-12-base-format-2012.pdf, page 56
class UrnBox : public FullBox {
public:
    UrnBox(uint32_t flags);
    virtual ~UrnBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    std::string name_;
    std::string location_;
};
};