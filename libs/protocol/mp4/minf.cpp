#include "minf.h"
#include "mp4_factory.h"
#include "base/net_buffer.h"
using namespace mms;

MinfBox::MinfBox() : Box(BOX_TYPE_MINF) {

}

MinfBox::~MinfBox() {

}

void MinfBox::add_box(std::shared_ptr<Box> child) {
    boxes_.push_back(child);
}

std::shared_ptr<Box> MinfBox::find_box(Box::Type type) {
    for (auto & box : boxes_) {
        if (box->type() == type) {
            return box;
        }
    }
    return nullptr;
}

int64_t MinfBox::size() {
    int64_t total_bytes = Box::size();
    for (auto c : boxes_) {
        total_bytes += c->size();
    }
    return total_bytes;
}

int64_t MinfBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    Box::encode(buf);
    for (auto & c : boxes_) {
        c->encode(buf);
    }
    return buf.pos() - start;
}

int64_t MinfBox::decode(NetBuffer & buf) {
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