#include "trak.h"
#include "tkhd.h"
#include "mdia.h"

#include "mp4_factory.h"
#include <string.h>

using namespace mms;

TrakBox::TrakBox() : Box(BOX_TYPE_TRAK) {

}

int64_t TrakBox::size() {
    int64_t total_bytes = Box::size();
    if (tkhd_) {
        total_bytes += tkhd_->size();
    }

    if (mdia_) {
        total_bytes += mdia_->size();
    }

    return total_bytes;
}

int64_t TrakBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    Box::encode(buf);
    
    if (tkhd_) {
        tkhd_->encode(buf);
    }

    if (mdia_) {
        mdia_->encode(buf);
    }
    return buf.pos() - start;
}

int64_t TrakBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Box::decode(buf);

    auto left_bytes = decoded_size() - (buf.pos() - start);
    while (left_bytes > 0) {
        auto [child, consumed] = MP4BoxFactory::decode_box(buf);
        if (child == nullptr || consumed <= 0) {
            return 0;
        }

        if (child->type() == BOX_TYPE_TKHD) {
            tkhd_ = std::static_pointer_cast<TkhdBox>(child);
        } else if (child->type() == BOX_TYPE_MDIA) {
            mdia_ = std::static_pointer_cast<MdiaBox>(child);
        }

        left_bytes -= consumed;
    }
    
    return buf.pos() - start;
}

void TrakBox::set_tkhd(std::shared_ptr<TkhdBox> tkhd) {
    tkhd_ = tkhd;
}

std::shared_ptr<TkhdBox> TrakBox::get_tkhd() {
    return tkhd_;
}

void TrakBox::set_mdia(std::shared_ptr<MdiaBox> mdia) {
    mdia_ = mdia;
}

std::shared_ptr<MdiaBox> TrakBox::get_mdia() {
    return mdia_;
}