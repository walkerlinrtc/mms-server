#pragma once
#include "full_box.h"
namespace mms {
class NetBuffer;
// 8.8.5 Movie Fragment Header Box (mfhd)
// ISO_IEC_14496-12-base-format-2012.pdf, page 67
// The movie fragment header contains a sequence number, as a safety check. The sequence number usually
// starts at 1 and must increase for each movie fragment in the file, in the order in which they occur. This allows
// readers to verify integrity of the sequence; it is an error to construct a file where the fragments are out of
// sequence.
class MfhdBox : public FullBox {
public:
    MfhdBox();
    virtual ~MfhdBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    // The ordinal number of this fragment, in increasing order
    uint32_t sequence_number_;
};
};