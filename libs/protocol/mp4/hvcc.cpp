#include "hvcc.h"
#include "mp4_factory.h"
#include <string.h>
#include "base/net_buffer.h"

using namespace mms;

HvccBox::HvccBox() : Box(BOX_TYPE_HVCC) {

}

int64_t HvccBox::size() {
    int64_t total_bytes = Box::size();
    total_bytes += hevc_config_.size();
    return total_bytes;
}

int64_t HvccBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    Box::encode(buf);
    buf.write_string(hevc_config_);
    return buf.pos() - start;
}

int64_t HvccBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Box::decode(buf);
    auto left_bytes = Box::decoded_size() - (buf.pos() - start);
    hevc_config_ = buf.read_string(left_bytes);
    return buf.pos() - start;
}