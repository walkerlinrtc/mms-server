#pragma once
#include "box.h"
namespace mms {
class TfhdBox;
class TfdtBox;
class TrunBox;
class NetBuffer;
// 8.8.6 Track Fragment Box (traf)
// ISO_IEC_14496-12-base-format-2012.pdf, page 67
// Within the movie fragment there is a set of track fragments, zero or more per track. The track fragments in
// turn contain zero or more track runs, each of which document a contiguous run of samples for that track.
// Within these structures, many fields are optional and can be defaulted.
class TrafBox : public Box {
public:
    TrafBox();
    virtual ~TrafBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    std::shared_ptr<TfhdBox> tfhd_;
    std::shared_ptr<TfdtBox> tfdt_;
    std::shared_ptr<TrunBox> trun_;
};
};