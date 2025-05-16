#include <string.h>
#include <cassert>

#include "visual_sample_entry.h"
#include "base/net_buffer.h"
#include "mp4_factory.h"
#include "avcc.h"
#include "hvcc.h"

using namespace mms;

VisualSampleEntry::VisualSampleEntry(Box::Type type) : SampleEntry(type) {
    
}

VisualSampleEntry::~VisualSampleEntry() {

}

int64_t VisualSampleEntry::size() {
    int64_t total_bytes = SampleEntry::size();
    // total_bytes += 74;
    total_bytes += 2;//predefined1
    total_bytes += 2;// reserved1
    total_bytes += 3*4;// pre_defined
    total_bytes += 2;//width
    total_bytes += 2;//height
    total_bytes += 4;//hor
    total_bytes += 4;//ver
    total_bytes += 4;//reserved;
    total_bytes += 2;//frame_count
    total_bytes += 32;//compare
    total_bytes += 2;//depth
    total_bytes += 2;//predefined

    if (avcc_) {
        total_bytes += avcc_->size();
    } else if (hvcc_) {
        total_bytes += hvcc_->size();
    }
    return total_bytes;
}

int64_t VisualSampleEntry::encode(NetBuffer& buf) {
    update_size();
    auto start = buf.pos();
    SampleEntry::encode(buf);
    // 基本视觉参数编码
    buf.write_2bytes(pre_defined1_);
    buf.write_2bytes(reserved1_);
    for (int i = 0; i < 3; ++i) {
        buf.write_4bytes(pre_defined2_[i]);
    }
    buf.write_2bytes(width_);
    buf.write_2bytes(height_);
    buf.write_4bytes(horizresolution_);
    buf.write_4bytes(vertresolution_);
    buf.write_4bytes(reserved2_);
    buf.write_2bytes(frame_count_);
    buf.write_string(std::string_view(compressorname_, 32)); // 固定32字节
    buf.write_2bytes(depth_);
    buf.write_2bytes(pre_defined3_);

    // 编码子box
    if (avcc_) avcc_->encode(buf);
    if (hvcc_) hvcc_->encode(buf);

    return buf.pos() - start;
}

int64_t VisualSampleEntry::decode(NetBuffer& buf) {
    auto start = buf.pos();
    SampleEntry::decode(buf);

    // 基本视觉参数解码
    pre_defined1_ = buf.read_2bytes();
    reserved1_ = buf.read_2bytes();
    if (reserved1_ != 0) {
    }

    for (int i = 0; i < 3; ++i) {
        pre_defined2_[i] = buf.read_4bytes();
    }

    width_ = buf.read_2bytes();
    height_ = buf.read_2bytes();
    horizresolution_ = buf.read_4bytes();
    vertresolution_ = buf.read_4bytes();
    reserved2_ = buf.read_4bytes();
    if (reserved2_ != 0) {
    }

    frame_count_ = buf.read_2bytes();
    buf.read_string(std::string_view(compressorname_, 32));

    depth_ = buf.read_2bytes();
    pre_defined3_ = buf.read_2bytes();

    // 解码子box
    while (buf.pos() - start < decoded_size()) {
        auto [box, consumed] = MP4BoxFactory::decode_box(buf);
        if (!box || consumed <= 0) {
            return 0;
        }

        if (box->type() == BOX_TYPE_AVCC) {
            avcc_ = std::static_pointer_cast<AvccBox>(box);
        } 
        else if (box->type() == BOX_TYPE_HVCC) {
            hvcc_ = std::static_pointer_cast<HvccBox>(box);
        }
    }

    return buf.pos() - start;
}

void VisualSampleEntry::set_avcc_box(std::shared_ptr<Box> avcc) {
    assert(avcc->type() == BOX_TYPE_AVCC);
    avcc_ = avcc;
}

void VisualSampleEntry::set_hvcc_box(std::shared_ptr<Box> hvcc) {
    assert(hvcc->type() == BOX_TYPE_HVCC);
    hvcc_ = hvcc;
}