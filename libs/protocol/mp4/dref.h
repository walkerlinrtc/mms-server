#pragma once
#include "full_box.h"
namespace mms {
class NetBuffer;

// 8.7.2 Data Reference Box (dref)
// ISO_IEC_14496-12-base-format-2012.pdf, page 56
// The data reference object contains a table of data references (normally URLs) that declare the location(s) of
// The media data used within the presentation. The data reference index in the sample description ties entries
// in this table to the samples in the track. A track may be split over several sources in this way.
class DrefBox : public FullBox {
public:
    DrefBox();
    virtual ~DrefBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    std::vector<std::shared_ptr<FullBox>> entries_;
};
};