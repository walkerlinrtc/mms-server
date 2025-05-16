#pragma once
#include "full_box.h"
namespace mms {
class NetBuffer;
// 8.7.3.2 Sample Size Box (stsz), for Audio/Video.
// ISO_IEC_14496-12-base-format-2012.pdf, page 58
// This box contains the sample count and a table giving the size in bytes of each sample. This allows the media data
// itself to be unframed. The total number of samples in the media is always indicated in the sample count.
class StszBox : public FullBox {
public:
    StszBox();
    virtual ~StszBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    // The default sample size. If all the samples are the same size, this field
    // contains that size value. If this field is set to 0, then the samples have different sizes, and those sizes
    // are stored in the sample size table. If this field is not 0, it specifies the constant sample size, and no
    // array follows.
    uint32_t sample_size_ = 0;
    // Each entry_size is an integer specifying the size of a sample, indexed by its number.
    std::vector<uint32_t> entry_sizes_;
};
};