#include "vmhd.h"
using namespace mms;

VmhdBox::VmhdBox() : FullBox(BOX_TYPE_VMHD, 0, 1) {

}

VmhdBox::~VmhdBox() {

}

int64_t VmhdBox::size() {
    int64_t total_bytes = FullBox::size();
    total_bytes += 8;
    return total_bytes;
}

int64_t VmhdBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);
    buf.write_2bytes(graphicsmode_);
    for (size_t i = 0; i < 3; i++) {
        buf.write_2bytes(opcolor_[i]);
    }
    return buf.pos() - start;
}

int64_t VmhdBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    graphicsmode_ = buf.read_2bytes();
    for (size_t i = 0; i < 3; i++) {
        opcolor_[i] = buf.read_2bytes();
    }
    return buf.pos() - start;
}