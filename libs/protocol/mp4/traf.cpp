#include "traf.h"
#include "tfhd.h"
#include "tfdt.h"
#include "trun.h"
#include "mp4_factory.h"
#include "spdlog/spdlog.h"

using namespace mms;

TrafBox::TrafBox() : Box(BOX_TYPE_TRAF) {

}

TrafBox::~TrafBox() {

}

int64_t TrafBox::size() {
    int64_t total_bytes = Box::size();
    if (tfhd_) {
        total_bytes += tfhd_->size();
    }
    
    if (tfdt_) {
        total_bytes += tfdt_->size();
    }

    if (trun_) {
        int64_t s = trun_->size();
        total_bytes += s;
    }
    return total_bytes;
}

int64_t TrafBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    Box::encode(buf);
    if (tfhd_) {
        tfhd_->encode(buf);
    }

    if (tfdt_) {
        tfdt_->encode(buf);
    }

    if (trun_) {
        trun_->encode(buf);
    }
    
    return buf.pos() - start;
}

int64_t TrafBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Box::decode(buf);
    
    auto left_bytes = decoded_size() - (buf.pos() - start);
    while (left_bytes > 0) {
        auto [box, consumed] = MP4BoxFactory::decode_box(buf);
        if (!box || consumed <= 0) {
            return -1;
        }

        if (box->type() == BOX_TYPE_TFHD) {
            tfhd_ = std::static_pointer_cast<TfhdBox>(box);
        } else if (box->type() == BOX_TYPE_TFDT) {
            tfdt_ = std::static_pointer_cast<TfdtBox>(box);
        } else if (box->type() == BOX_TYPE_TRUN) {
            trun_ = std::static_pointer_cast<TrunBox>(box);
        }
        left_bytes -= consumed;
    }

    return buf.pos() - start;
}