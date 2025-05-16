#include "full_box.h"
using namespace mms;

FullBox::FullBox(Type t, uint8_t version, uint32_t flags) : Box(t), version_(version), flags_(flags) {

}

int64_t FullBox::size() {
    int64_t total_bytes = Box::size() + 4;
    return total_bytes;
}

int64_t FullBox::encode(NetBuffer & buf) {
    auto start = buf.pos();
    Box::encode(buf);
    buf.write_1bytes(version_);
    buf.write_3bytes(flags_);
    return buf.pos() - start;
}

int64_t FullBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Box::decode(buf);
    version_ = buf.read_1bytes();
    flags_ = buf.read_3bytes();
    return buf.pos() - start;
}