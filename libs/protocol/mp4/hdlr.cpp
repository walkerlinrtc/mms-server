#include <string.h>
#include "hdlr.h"

using namespace mms;
HdlrBox::HdlrBox() : FullBox(BOX_TYPE_HDLR, 0, 0) {

}

HdlrBox::~HdlrBox() {

}

int64_t HdlrBox::size() {
    int64_t total_bytes = FullBox::size();
    return total_bytes + 20 + name_.size() + 1;
}

int64_t HdlrBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    pre_defined_ = buf.read_4bytes();
    handler_type_ = buf.read_4bytes();
    for (int i = 0; i < 3; i++) {
        reserved_[i] = buf.read_4bytes();
    }

    auto left_bytes = decoded_size() - (buf.pos() - start);
    name_ = buf.read_string(left_bytes);
    buf.skip(1);

    return buf.pos() - start;
}

int64_t HdlrBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);
    buf.write_4bytes(pre_defined_);
    buf.write_4bytes(handler_type_);
    buf.skip(12);
    buf.write_string(name_);
    buf.write_1bytes(0);

    return buf.pos() - start;
}