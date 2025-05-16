#pragma once
#include "full_box.h"
namespace mms {
// 8.6.2 Sync Sample Box (stss), for Video.
// ISO_IEC_14496-12-base-format-2012.pdf, page 51
// This box provides a compact marking of the sync samples within the stream. The table is arranged in strictly
// increasing order of sample number.
class NetBuffer;
class StssBox : public FullBox {
public:
    StssBox();
    virtual ~StssBox();
public:
    bool is_sync(uint32_t sample_index);
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    std::vector<uint32_t> sample_entries_;
};
};