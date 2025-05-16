#include "stsd.h"
#include "mp4_factory.h"
#include "sample_entry.h"
#include "visual_sample_entry.h"
#include "audio_sample_entry.h"
#include "base/net_buffer.h"

#include <string.h>
using namespace mms;

StsdBox::StsdBox() : FullBox(BOX_TYPE_STSD, 0, 0) {

}

StsdBox::~StsdBox() {

}

int64_t StsdBox::size() {
    int64_t total_bytes = FullBox::size() + 4;
    for (auto it = entries_.begin(); it != entries_.end(); it++) {
        total_bytes += (*it)->size();
    }
    return total_bytes;
}

int64_t StsdBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);
    buf.write_4bytes(entries_.size());
    for (size_t i = 0; i < entries_.size(); i++) {
        entries_[i]->encode(buf);
    }

    return buf.pos() - start;
}

int64_t StsdBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    uint32_t entry_count = buf.read_4bytes();
    for (size_t i = 0; i < entry_count; i++) {
        auto [box, consumed] = MP4BoxFactory::decode_box(buf);
        entries_.push_back(std::static_pointer_cast<SampleEntry>(box));
    }
    
    return buf.pos() - start;
}