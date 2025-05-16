#include <string.h>
#include "base/net_buffer.h"
#include "audio_sample_entry.h"
#include "esds.h"
using namespace mms;

AudioSampleEntry::AudioSampleEntry(Box::Type type) : SampleEntry(type) {
    
}

AudioSampleEntry::~AudioSampleEntry() {

}

int64_t AudioSampleEntry::size() {
    int64_t total_bytes = SampleEntry::size();
    total_bytes += 20;
    if (esds_) {
        total_bytes += esds_->size();
    }
    return total_bytes;
}

#include "spdlog/spdlog.h"
int64_t AudioSampleEntry::encode(NetBuffer & buf) {
    auto start = buf.pos();
    SampleEntry::encode(buf);

    buf.skip(8);
    buf.write_2bytes(channel_count_);
    buf.write_2bytes(sample_size_);
    buf.write_2bytes(pre_defined_);
    buf.skip(2);
    buf.write_4bytes(sample_rate_);
    if (esds_) {
        esds_->encode(buf);
    }

    return buf.pos() - start;
}

int64_t AudioSampleEntry::decode(NetBuffer & buf) {
    auto start = buf.pos();
    SampleEntry::decode(buf);
    buf.skip(8);
    channel_count_ = buf.read_2bytes();
    sample_size_ = buf.read_2bytes();
    pre_defined_ = buf.read_2bytes();
    buf.skip(2);
    sample_rate_ = buf.read_4bytes();
    auto left_bytes = decoded_size() - (buf.pos() - start);
    if (left_bytes > 0) {
        esds_ = std::make_shared<EsdsBox>();
        esds_->decode(buf);
    }

    return buf.pos() - start;
}