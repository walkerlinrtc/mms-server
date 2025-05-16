#include "tfdt.h"
#include "base/net_buffer.h"
using namespace mms;

TfdtBox::TfdtBox() : FullBox(BOX_TYPE_TFDT, 0, 0) {

}

TfdtBox::~TfdtBox() {

}

int64_t TfdtBox::size() {
    return FullBox::size() + (version_?8:4);
}

int64_t TfdtBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);
    if (version_) {
        buf.write_8bytes(base_media_decode_time);
    } else {
        buf.write_4bytes(base_media_decode_time);
    }
    return buf.pos() - start;
}

int64_t TfdtBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    if (version_) {
        base_media_decode_time = buf.read_8bytes();
    } else {
        base_media_decode_time = buf.read_4bytes();
    }
    return buf.pos() - start;
}