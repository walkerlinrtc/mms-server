#pragma once
#include "full_box.h"
namespace mms {
class UrlBox;
class UrnBox;
class DrefBox;
class NetBuffer;
// 8.7.1 Data Information Box (dinf)
// ISO_IEC_14496-12-base-format-2012.pdf, page 56
// The data information box contains objects that declare the location of the media information in a track.
class DinfBox : public Box {
public:
    DinfBox();
    virtual ~DinfBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;

    void set_dref(std::shared_ptr<DrefBox> dref) {
        dref_ = dref;
    }
    
    std::shared_ptr<DrefBox> get_dref() {
        return dref_;
    }
public:
    std::shared_ptr<DrefBox> dref_;
};
};