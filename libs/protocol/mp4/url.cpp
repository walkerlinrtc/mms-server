#include "url.h"
#include <string.h>
#include "base/net_buffer.h"
using namespace mms;

UrlBox::UrlBox() : FullBox(BOX_TYPE_URL, 0, 0) {

}

UrlBox::~UrlBox() {

}

int64_t UrlBox::size() {
    int64_t total_bytes = FullBox::size();
    total_bytes += location_.size() + 1;
    return total_bytes;
}

int64_t UrlBox::encode(NetBuffer & buf) {
    auto start = buf.pos();
    if (location_.empty()) {
        flags_ = 0x01;
    }
    update_size();
    FullBox::encode(buf);
    buf.write_string(location_);
    buf.write_1bytes(0);
    return buf.pos() - start;
}

int64_t UrlBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    int64_t left_bytes = decoded_size() - (buf.pos() - start);
    location_ = buf.read_string(left_bytes);
    buf.skip(1);
    return buf.pos() - start;
}