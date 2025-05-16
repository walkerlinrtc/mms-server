#include "avcc.h"
#include "mp4_factory.h"
#include "base/net_buffer.h"
#include <string.h>

using namespace mms;

AvccBox::AvccBox() : Box(BOX_TYPE_AVCC) {

}

int64_t AvccBox::size() {
    int64_t total_bytes = Box::size();
    total_bytes += avc_config_.size();
    return total_bytes;
}

int64_t AvccBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    Box::encode(buf);
    buf.write_string(avc_config_);
    return buf.pos() - start;
}

int64_t AvccBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Box::decode(buf);
    auto left_bytes = Box::decoded_size() - (buf.pos() - start);
    avc_config_ = buf.read_string(left_bytes);
    return buf.pos() - start;
}