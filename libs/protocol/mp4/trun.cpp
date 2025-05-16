#include "trun.h"
#include "base/net_buffer.h"
#include "spdlog/spdlog.h"
using namespace mms;

TrunEntry::TrunEntry(FullBox* o) {
    owner_ = o;
}

TrunEntry::~TrunEntry() {

}

int64_t TrunEntry::size() {
    int64_t size = 0;
    if ((owner_->flags_&TrunFlagsSampleDuration) == TrunFlagsSampleDuration) {
        size += 4;
    }
    if ((owner_->flags_&TrunFlagsSampleSize) == TrunFlagsSampleSize) {
        size += 4;
    }
    if ((owner_->flags_&TrunFlagsSampleFlag) == TrunFlagsSampleFlag) {
        size += 4;
    }
    if ((owner_->flags_&TrunFlagsSampleCtsOffset) == TrunFlagsSampleCtsOffset) {
        size += 4;
    }
    return size;
}

int64_t TrunEntry::encode(NetBuffer & buf) {
    auto start = buf.pos();
    if ((owner_->flags_&TrunFlagsSampleDuration) == TrunFlagsSampleDuration) {
        buf.write_4bytes(sample_duration_);
    }

    if ((owner_->flags_&TrunFlagsSampleSize) == TrunFlagsSampleSize) {
        buf.write_4bytes(sample_size_);
    }

    if ((owner_->flags_&TrunFlagsSampleFlag) == TrunFlagsSampleFlag) {
        buf.write_4bytes(sample_flags_);
    }

    if ((owner_->flags_&TrunFlagsSampleCtsOffset) == TrunFlagsSampleCtsOffset) {
        buf.write_4bytes(sample_composition_time_offset_);
    }

    return buf.pos() - start;
}

int64_t TrunEntry::decode(NetBuffer & buf) {
    auto start = buf.pos();
    if ((owner_->flags_&TrunFlagsSampleDuration) == TrunFlagsSampleDuration) {
        sample_duration_ = buf.read_4bytes();
    }

    if ((owner_->flags_&TrunFlagsSampleSize) == TrunFlagsSampleSize) {
        sample_size_ = buf.read_4bytes();
    }

    if ((owner_->flags_&TrunFlagsSampleFlag) == TrunFlagsSampleFlag) {
        sample_flags_ = buf.read_4bytes();
    }

    if ((owner_->flags_&TrunFlagsSampleCtsOffset) == TrunFlagsSampleCtsOffset) {
        if (!owner_->version_) {
            sample_composition_time_offset_ = buf.read_4bytes();
        } else {
            sample_composition_time_offset_ = (int32_t)buf.read_4bytes();
        }
    }
    
    return buf.pos() - start;
}

TrunBox::TrunBox(uint32_t flags) : FullBox(BOX_TYPE_TRUN, 0, flags) {

}

TrunBox::~TrunBox() {

}

int64_t TrunBox::size() {
    int64_t total_bytes = FullBox::size();
    total_bytes += 4;
    
    if ((flags_&TrunFlagsDataOffset) == TrunFlagsDataOffset) {
        total_bytes += 4;
    }

    if ((flags_&TrunFlagsFirstSample) == TrunFlagsFirstSample) {
        total_bytes += 4;
    }
    
    for (auto & e : entries_) {
        total_bytes += e.size();
    }

    return total_bytes;
}

int64_t TrunBox::encode(NetBuffer & buf) {
    auto start = buf.pos();
    update_size();
    FullBox::encode(buf);

    uint32_t entry_count = (uint32_t)entries_.size();
    buf.write_4bytes(entry_count);

    if ((flags_&TrunFlagsDataOffset) == TrunFlagsDataOffset) {
        buf.write_4bytes(data_offset_);
    }

    if ((flags_&TrunFlagsFirstSample) == TrunFlagsFirstSample) {
        buf.write_4bytes(first_sample_flags_);
    }

    for (auto & e : entries_) {
        e.encode(buf);
    }

    return buf.pos() - start;
}

int64_t TrunBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    uint32_t entry_count = buf.read_4bytes();
    if ((flags_&TrunFlagsDataOffset) == TrunFlagsDataOffset) {
        data_offset_ = buf.read_4bytes();
    }

    if ((flags_&TrunFlagsFirstSample) == TrunFlagsFirstSample) {
        first_sample_flags_ = buf.read_4bytes();
    }
    
    for (int i = 0; i < (int)entry_count; i++) {
        TrunEntry te(this);
        te.decode(buf);
        entries_.push_back(te);
    }
    
    return buf.pos() - start;
}