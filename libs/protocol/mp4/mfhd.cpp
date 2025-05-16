#include "mfhd.h"
using namespace mms;

MfhdBox::MfhdBox() : FullBox(BOX_TYPE_MFHD, 0, 0) {

}

MfhdBox::~MfhdBox() {

}

int64_t MfhdBox::size() {
    int64_t total_bytes = FullBox::size();
    total_bytes += 4;
    return total_bytes;
}

int64_t MfhdBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);
    buf.write_4bytes(sequence_number_);
    return buf.pos() - start;
}

int64_t MfhdBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    sequence_number_ = buf.read_4bytes();
    return buf.pos() - start;
}
