#include "ftyp.h"
#include <string.h>
#include <cassert>
#include "base/net_buffer.h"
#include "mp4_builder.h"
#include "spdlog/spdlog.h"
using namespace mms;

FtypBox::FtypBox() : Box(BOX_TYPE_FTYP) {
    // 默认值
    major_brand_ = Mp4BoxBrandISO5;
    minor_version_ = 512;
}

int64_t FtypBox::size() {
    int64_t total_bytes = Box::size() + 4 + sizeof(minor_version_);
    total_bytes += compatible_brands_.size()*4;
    return total_bytes;
}

int64_t FtypBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    Box::encode(buf);
    buf.write_4bytes(major_brand_);
    buf.write_4bytes(minor_version_);
    for (const auto& brand : compatible_brands_) {
        buf.write_4bytes(brand);
    }
    return buf.pos() - start;
}

int64_t FtypBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    Box::decode(buf);
    major_brand_ = buf.read_4bytes();
    minor_version_ = buf.read_4bytes();
    auto left_bytes = decoded_size() - (buf.pos() - start);
    while (left_bytes > 0) {
        auto s = buf.read_4bytes();
        compatible_brands_.push_back(s);
        left_bytes -= 4;
    }

    return buf.pos() - start;
}


FtypBuilder::FtypBuilder(Mp4Builder & builder) : builder_(builder) {
    builder_.begin_box(BOX_TYPE_FTYP);
}

FtypBuilder::~FtypBuilder() {
    builder_.end_box();
}

FtypBuilder & FtypBuilder::set_major_brand(uint32_t v) {
    auto & cm = builder_.get_chunk_manager();
    cm.write_4bytes(v);
    return *this;
}

FtypBuilder & FtypBuilder::set_minor_version(uint32_t v) {
    auto & cm = builder_.get_chunk_manager();
    cm.write_4bytes(v);
    return *this;
}

FtypBuilder & FtypBuilder::set_compatible_brands(const std::vector<uint32_t> & brands) {
    auto & cm = builder_.get_chunk_manager();
    for (auto & b : brands) {
        cm.write_4bytes(b);
    }
    return *this;
}