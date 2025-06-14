#include "urn.h"
#include <string.h>
#include "base/net_buffer.h"

using namespace mms;

UrnBox::UrnBox(uint32_t flags) : FullBox(BOX_TYPE_URN, 0, flags) {

}

UrnBox::~UrnBox() {

}

int64_t UrnBox::size() {
    int64_t total_bytes = FullBox::size();
    total_bytes += name_.size() + 1;
    total_bytes += location_.size() + 1;
    return total_bytes;
}

int64_t UrnBox::encode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::encode(buf);
    buf.write_string(name_);
    buf.write_1bytes(0);
    buf.write_string(location_);
    buf.write_1bytes(0);
    return buf.pos() - start;
}

int64_t UrnBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    // int64_t left_bytes = decoded_size() - (buf.pos() - start);
    auto curr_buf = buf.get_curr_buf();
    auto name_len = strlen(curr_buf.data());
    name_ = buf.read_string(name_len);
    buf.skip(1);
    curr_buf = buf.get_curr_buf();
    auto location_len = strlen(curr_buf.data());
    location_ = buf.read_string(location_len);
    buf.skip(1);
    return buf.pos() - start;
}