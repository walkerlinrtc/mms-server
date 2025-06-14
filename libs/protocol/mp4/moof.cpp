#include "moof.h"
#include "mdhd.h"
#include "mfhd.h"
#include "traf.h"
#include "mp4_factory.h"
#include "base/net_buffer.h"
#include "spdlog/spdlog.h"

using namespace mms;

MoofBox::MoofBox() : Box(BOX_TYPE_MOOF) {

}

MoofBox::~MoofBox() {

}

int64_t MoofBox::size() {
    int64_t total_bytes = Box::size();
    // if (mdhd_) {
    //     int64_t s = mdhd_->size();
    //     total_bytes += s;
    // }

    if (mfhd_) {
        int64_t s = mfhd_->size();
        total_bytes += s;
    }

    if (traf_) {
        int64_t s = traf_->size();
        total_bytes += s;
    }
    return total_bytes;
}

int64_t MoofBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    Box::encode(buf);

    // if (mdhd_) {
    //     mdhd_->encode(buf);
    // }

    if (mfhd_) {
        mfhd_->encode(buf);
    }

    if (traf_) {
        traf_->encode(buf);
    }

    return buf.pos() - start;
}

int64_t MoofBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Box::decode(buf);
    auto left_bytes = decoded_size() - (buf.pos() - start);
    while (left_bytes > 0) {
        auto [child, consumed] = MP4BoxFactory::decode_box(buf);
        if (child == nullptr || consumed <= 0) {
            return 0;
        }

        // if (child->type() == BOX_TYPE_MDHD) {
        //     mdhd_ = std::static_pointer_cast<MdhdBox>(child);
        // } else 
        if (child->type() == BOX_TYPE_MFHD) {
            mfhd_ = std::static_pointer_cast<MfhdBox>(child);
        } else if (child->type() == BOX_TYPE_TRAF) {
            traf_ = std::static_pointer_cast<TrafBox>(child);
        }

        left_bytes -= consumed;
    }
    
    return buf.pos() - start;
}