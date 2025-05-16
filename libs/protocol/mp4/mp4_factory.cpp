#include "base/net_buffer.h"

#include "mp4_factory.h"

#include "box.h"
#include "mvhd.h"
#include "moov.h"
#include "avcc.h"
#include <arpa/inet.h>

using namespace mms;
MP4BoxFactory::MP4BoxFactory() {

}

std::pair<std::shared_ptr<Box>, int64_t> MP4BoxFactory::decode_box(NetBuffer & buf) {
    auto b = buf.get_curr_buf();
    if (b.size() < 8) {
        return {nullptr, 0};
    }

    Box::Type t = (Box::Type)(htonl(*(uint32_t*)(b.data() + 4)));
    std::shared_ptr<Box> box;
    int64_t consumed = 0;
    auto start = buf.pos();
    switch(t) {
        case BOX_TYPE_MVHD:box = std::make_shared<MvhdBox>();consumed = box->decode(buf);break;
        case BOX_TYPE_MOOV:box = std::make_shared<MoovBox>();consumed = box->decode(buf);break;
        case BOX_TYPE_AVCC:box = std::make_shared<AvccBox>();consumed = box->decode(buf);break;
        default:break;
    }

    if (consumed <= 0) {
        return {nullptr, 0};
    }

    return {box, buf.pos() - start};
}