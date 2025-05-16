#include "tfhd.h"
#include "base/net_buffer.h"
using namespace mms;

TfhdBox::TfhdBox(uint32_t flags) : FullBox(BOX_TYPE_TFHD, 0, flags) {

}

TfhdBox::~TfhdBox() {

}

int64_t TfhdBox::size() {
    int64_t total_bytes = FullBox::size() + 4;
    if ((flags_&TfhdFlagsBaseDataOffset) == TfhdFlagsBaseDataOffset) {
        total_bytes += 8;
    }
    if ((flags_&TfhdFlagsSampleDescriptionIndex) == TfhdFlagsSampleDescriptionIndex) {
        total_bytes += 4;
    }
    if ((flags_&TfhdFlagsDefaultSampleDuration) == TfhdFlagsDefaultSampleDuration) {
        total_bytes += 4;
    }
    if ((flags_&TfhdFlagsDefautlSampleSize) == TfhdFlagsDefautlSampleSize) {
        total_bytes += 4;
    }
    if ((flags_&TfhdFlagsDefaultSampleFlags) == TfhdFlagsDefaultSampleFlags) {
        total_bytes += 4;
    }
    return total_bytes;
}

int64_t TfhdBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);
    buf.write_4bytes(track_id_);
    
    if ((flags_&TfhdFlagsBaseDataOffset) == TfhdFlagsBaseDataOffset) {
        buf.write_8bytes(base_data_offset_);
    }

    if ((flags_&TfhdFlagsSampleDescriptionIndex) == TfhdFlagsSampleDescriptionIndex) {
        buf.write_4bytes(sample_description_index_);
    }

    if ((flags_&TfhdFlagsDefaultSampleDuration) == TfhdFlagsDefaultSampleDuration) {
        buf.write_4bytes(default_sample_duration_);
    }

    if ((flags_&TfhdFlagsDefautlSampleSize) == TfhdFlagsDefautlSampleSize) {
        buf.write_4bytes(default_sample_size_);
    }

    if ((flags_&TfhdFlagsDefaultSampleFlags) == TfhdFlagsDefaultSampleFlags) {
        buf.write_4bytes(default_sample_flags_);
    }

    return buf.pos() - start;
}

int64_t TfhdBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    track_id_ = buf.read_4bytes();

    if ((flags_&TfhdFlagsBaseDataOffset) == TfhdFlagsBaseDataOffset) {
        base_data_offset_ = buf.read_8bytes();
    }

    if ((flags_&TfhdFlagsSampleDescriptionIndex) == TfhdFlagsSampleDescriptionIndex) {
        sample_description_index_ = buf.read_4bytes();
    }

    if ((flags_&TfhdFlagsDefaultSampleDuration) == TfhdFlagsDefaultSampleDuration) {
        default_sample_duration_ = buf.read_4bytes();
    }

    if ((flags_&TfhdFlagsDefautlSampleSize) == TfhdFlagsDefautlSampleSize) {
        default_sample_size_ = buf.read_4bytes();
    }

    if ((flags_&TfhdFlagsDefaultSampleFlags) == TfhdFlagsDefaultSampleFlags) {
        default_sample_flags_ = buf.read_4bytes();
    }

    return buf.pos() - start;
}