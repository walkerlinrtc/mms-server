#include "mvex.h"
#include "trex.h"
#include "mp4_factory.h"
#include "base/net_buffer.h"
using namespace mms;

MvexBox::MvexBox() : Box(BOX_TYPE_MVEX) {

}

MvexBox::~MvexBox() {

}

int64_t MvexBox::size() {
    int64_t total_bytes = Box::size();
    for (auto box : boxes_) {
        total_bytes += box->size();
    }
    return total_bytes;
}

int64_t MvexBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    Box::encode(buf);
    for (auto box : boxes_) {
        box->encode(buf);
    }

    return buf.pos() - start;
}

int64_t MvexBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Box::decode(buf);
    auto left_bytes = decoded_size() - (buf.pos() - start);
    while (left_bytes > 0) {
        auto [child, consumed] = MP4BoxFactory::decode_box(buf);
        if (child == nullptr || consumed <= 0) {
            return 0;
        }
        boxes_.push_back(child);
        left_bytes -= consumed;
    }
    
    return buf.pos() - start;
}

