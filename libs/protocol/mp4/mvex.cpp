#include "mvex.h"
#include "trex.h"
#include "base/net_buffer.h"
using namespace mms;

MvexBox::MvexBox() : Box(BOX_TYPE_MVEX) {

}

MvexBox::~MvexBox() {

}

int64_t MvexBox::size() {
    int64_t total_bytes = Box::size();
    if (trex_) {
        total_bytes += trex_->size();
    }
    return total_bytes;
}

int64_t MvexBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    Box::encode(buf);
    if (trex_) {
        trex_->encode(buf);
    }
    return buf.pos() - start;
}

int64_t MvexBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Box::decode(buf);
    if (trex_) {
        trex_->decode(buf);
    }
    return buf.pos() - start;
}

