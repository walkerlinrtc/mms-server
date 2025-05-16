#include <string.h>

#include "mp4_factory.h"
#include "url.h"
#include "urn.h"
#include "dref.h"

using namespace mms;
DrefBox::DrefBox() : FullBox(BOX_TYPE_DREF, 0, 0) {

}

DrefBox::~DrefBox() {

}

int64_t DrefBox::size() {
    int64_t total_bytes = FullBox::size();
    total_bytes += 4;
    for (auto & e : entries_) {
        total_bytes += e->size();
    }
    return total_bytes;
}

int64_t DrefBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);
    buf.write_4bytes(entries_.size());
    for (auto & e : entries_) {
        e->encode(buf);
    }
    return buf.pos() - start;
}

int64_t DrefBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    size_t entry_count = buf.read_4bytes();
    for (size_t i = 0; i < entry_count; i++) {
        auto [box, consumed] = MP4BoxFactory::decode_box(buf);
        if (box) {
            if (box->type() == BOX_TYPE_URL) {
                entries_.push_back(std::static_pointer_cast<UrlBox>(box));
            } else if (box->type() == BOX_TYPE_URN) {
                entries_.push_back(std::static_pointer_cast<UrnBox>(box));
            } else if (box->type() == BOX_TYPE_DREF) {
                entries_.push_back(std::static_pointer_cast<DrefBox>(box));
            }
        } else {
            return  -1;
        }
    }

    return buf.pos() - start;
}