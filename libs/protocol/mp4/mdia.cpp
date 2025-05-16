#include "mdia.h"
#include "hdlr.h"
#include "minf.h"
#include "mdhd.h"
#include "mp4_factory.h"

using namespace mms;

MdiaBox::MdiaBox() : Box(BOX_TYPE_MDIA) {

}

MdiaBox::~MdiaBox() {

}

int64_t MdiaBox::size() {
    int64_t total_bytes = Box::size();
    if (hdlr_) {
        total_bytes += hdlr_->size();
    }

    if (mdhd_) {
        total_bytes += mdhd_->size();
    }

    if (minf_) {
        total_bytes += minf_->size();
    }
    return total_bytes;
}

int64_t MdiaBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    Box::encode(buf);
    if (hdlr_) {
        hdlr_->encode(buf);
    }

    if (mdhd_) {
        mdhd_->encode(buf);
    }

    if (minf_) {
        minf_->encode(buf);
    }
    return buf.pos() - start;
}

int64_t MdiaBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Box::decode(buf);
    auto left_bytes = decoded_size() - (buf.pos() - start);
    while (left_bytes > 0) {
        auto [box, consumed] = MP4BoxFactory::decode_box(buf);
        if (box == nullptr || consumed <= 0) {
            return -1;
        }

        if (box->type() == BOX_TYPE_MINF) {
            minf_ = std::static_pointer_cast<MinfBox>(box);
        } else if (box->type() == BOX_TYPE_MDHD) {
            mdhd_ = std::static_pointer_cast<MdhdBox>(box);
        } else if (box->type() == BOX_TYPE_HDLR) {
            hdlr_ = std::static_pointer_cast<HdlrBox>(box);
        }
        left_bytes -= consumed;
    }
    return buf.pos() - start;
}