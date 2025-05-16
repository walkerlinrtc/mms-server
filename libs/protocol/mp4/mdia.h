#pragma once
#include "box.h"
#include <memory>
namespace mms {
class HdlrBox;
class MdhdBox;
class MinfBox;
class NetBuffer;
// 8.4.1 Media Box (mdia)
// ISO_IEC_14496-12-base-format-2012.pdf, page 36
// The media declaration container contains all the objects that declare information about the media data within a
// track.
class MdiaBox : public Box {
public:
    MdiaBox();
    virtual ~MdiaBox();
public:
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    void set_hdlr(std::shared_ptr<HdlrBox> hdlr) {
        hdlr_ = hdlr;
    }

    std::shared_ptr<HdlrBox> get_hdlr() {
        return hdlr_;
    }

    void set_mdhd(std::shared_ptr<MdhdBox> mdhd) {
        mdhd_ = mdhd;
    }

    std::shared_ptr<MdhdBox> get_mdhd() {
        return mdhd_;
    }

    void set_minf(std::shared_ptr<MinfBox> minf) {
        minf_ = minf;
    }

    std::shared_ptr<MinfBox> get_minf() {
        return minf_;
    }
protected:
    std::shared_ptr<HdlrBox> hdlr_;
    std::shared_ptr<MdhdBox> mdhd_;
    std::shared_ptr<MinfBox> minf_;
};
};