#pragma once
#include "box.h"

namespace mms {
class MfhdBox;
class TrafBox;
class MdhdBox;

class MoofBox : public Box {
public:
    MoofBox();
    virtual ~MoofBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    std::shared_ptr<MfhdBox> get_mfhd() {
        return mfhd_;
    }

    // void set_mfhd(std::shared_ptr<MfhdBox> mfhd) {
    //     mfhd_ = mfhd;
    // }

    void set_traf(std::shared_ptr<TrafBox> traf) {
        traf_ = traf;
    }

    std::shared_ptr<TrafBox> get_traf() {
        return traf_;
    }
public:
    // std::shared_ptr<MdhdBox> mdhd_;
    std::shared_ptr<MfhdBox> mfhd_;
    std::shared_ptr<TrafBox> traf_;
};

};