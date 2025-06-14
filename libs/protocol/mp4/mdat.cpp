#include "mdat.h"
#include "base/net_buffer.h"
using namespace mms;

MdatBox::MdatBox() : Box(BOX_TYPE_MDAT) {

}

MdatBox::~MdatBox() {

}

int64_t MdatBox::size() {
    int64_t total_bytes = Box::size();
    for (const auto& data : datas_) {
        total_bytes += data.size();
    }
    return total_bytes;
}

int64_t MdatBox::encode(NetBuffer& buf) {
    update_size();
    auto start = buf.pos();
    Box::encode(buf);
    for (const auto& data : datas_) {
        if (!data.empty()) {
            buf.write_string(data);
        }
    }
    return buf.pos() - start;
}
