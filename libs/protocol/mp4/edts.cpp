#include "edts.h"
#include "base/net_buffer.h"
#include "elst.h"
using namespace mms;

EdtsBox::EdtsBox() : Box(BOX_TYPE_EDTS) {

}

EdtsBox::~EdtsBox() {

}

void EdtsBox::set_elst(std::shared_ptr<ElstBox> elst) {
    elst_ = elst;
}

std::shared_ptr<ElstBox> EdtsBox::get_elst() {
    return elst_;
}

int64_t EdtsBox::size() {
    int64_t total_bytes = Box::size();
    if (elst_) {
        total_bytes += elst_->size();
    }
    return total_bytes;
}

int64_t EdtsBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    Box::encode(buf);
    if (elst_) {
        elst_->encode(buf);
    }
    return buf.pos() - start;
}

int64_t EdtsBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Box::decode(buf);
    auto left_bytes = decoded_size() - (buf.pos() - start);
    if (left_bytes <= 0) {
        return 0;
    }
    elst_ = std::make_shared<ElstBox>();
    elst_->decode(buf);
    return buf.pos() - start;
}