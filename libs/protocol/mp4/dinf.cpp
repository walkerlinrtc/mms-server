#include "dinf.h"
#include "url.h"
#include "urn.h"
#include "dref.h"
#include "base/net_buffer.h"
#include <cassert>

using namespace mms;

DinfBox::DinfBox() : Box(BOX_TYPE_DINF) {

}

DinfBox::~DinfBox() {

}

int64_t DinfBox::size() {
    assert(dref_ != nullptr);
    int64_t total_bytes = Box::size();
    if (dref_) {
        total_bytes += dref_->size();
    }

    return total_bytes;
}

int64_t DinfBox::encode(NetBuffer & buf) {
    assert(dref_ != nullptr);
    update_size();
    auto start = buf.pos();
    Box::encode(buf);
    if (dref_) {
        dref_->encode(buf);
    }
    return buf.pos() - start;
}

int64_t DinfBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Box::decode(buf);
    auto left_bytes = decoded_size() - (buf.pos() - start);
    if (left_bytes <= 0) {
        return 0;
    }
    dref_ = std::make_shared<DrefBox>();
    dref_->decode(buf);
    return buf.pos() - start;
}