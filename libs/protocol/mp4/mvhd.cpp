#include "mvhd.h"
#include <string.h>
#include "base/net_buffer.h"
#include "mp4_builder.h"
using namespace mms;

MvhdBox::MvhdBox() : FullBox(BOX_TYPE_MVHD, 0, 0) {

}

int64_t MvhdBox::size() {
    // todo
    int64_t total_bytes = FullBox::size();
    if (version_ == 1) {
        total_bytes += 8*3 + 4;
    } else {
        total_bytes += 4*4;
    }

    total_bytes += 6;//4(rate) + 2(volume)
    total_bytes += 10;//10(reserved)
    total_bytes += 9*4;//matrix(9*4)
    total_bytes += 24;//pre_defined(6*4)
    total_bytes += 4;//next_track_ID(4)
    return total_bytes;
}

int64_t MvhdBox::encode(NetBuffer & buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);

    if (version_ == 1) {
        buf.write_8bytes(creation_time_);
        buf.write_8bytes(modification_time_);
        buf.write_4bytes(timescale_);
        buf.write_8bytes(duration_);
    } else {
        buf.write_4bytes(creation_time_);
        buf.write_4bytes(modification_time_);
        buf.write_4bytes(timescale_);
        buf.write_4bytes(duration_);
    }

    buf.write_4bytes(rate_);
    buf.write_2bytes(volume_);
    buf.skip(10);
    for (int i = 0; i < 9; i++) {
        buf.write_4bytes(matrix_[i]);
    }
    buf.skip(24);
    buf.write_4bytes(next_track_ID_);
    return buf.pos() - start;
}

int64_t MvhdBox::decode(NetBuffer & buf) {
    auto start = buf.pos();
    FullBox::decode(buf);
    if (version_ == 1) {
        creation_time_ = buf.read_8bytes();
        modification_time_ = buf.read_8bytes();
        timescale_ = buf.read_8bytes();
        duration_ = buf.read_8bytes();
    } else {
        creation_time_ = buf.read_4bytes();
        modification_time_ = buf.read_4bytes();
        timescale_ = buf.read_4bytes();
        duration_ = buf.read_4bytes();
    }

    rate_ = buf.read_4bytes();
    volume_ = buf.read_2bytes();
    buf.skip(10);
    for (int i = 0; i < 9; i++) {
        matrix_[i] = buf.read_4bytes();
    }    
    buf.skip(24);

    next_track_ID_ = buf.read_4bytes();
    return buf.pos() - start;
}

// ---- 实现 ----
MvhdBuilder::MvhdBuilder(Mp4Builder & builder) : builder_(builder) {
    builder_.begin_box(BOX_TYPE_MVHD);
}

MvhdBuilder::~MvhdBuilder() {
    builder_.end_box();
}

MvhdBuilder& MvhdBuilder::set_version(uint8_t version) {
    version_ = version;
    auto& cm = builder_.get_chunk_manager();
    cm.write_1bytes(version_);
    return *this;
}

MvhdBuilder& MvhdBuilder::set_flags(uint32_t flags) {
    auto& cm = builder_.get_chunk_manager();
    // 回填 flags 到第 2-4 字节（跳过 version）
    cm.write_3bytes(flags & 0x00FFFFFF); 
    return *this;
}

MvhdBuilder& MvhdBuilder::set_creation_time(uint64_t time) {
    auto& cm = builder_.get_chunk_manager();
    if (version_ == 1) {
        cm.write_8bytes(time);
    } else {
        cm.write_4bytes(static_cast<uint32_t>(time));
    }
    return *this;
}

MvhdBuilder& MvhdBuilder::set_modification_time(uint64_t time) {
    auto& cm = builder_.get_chunk_manager();
    if (version_ == 1) {
        cm.write_8bytes(time);
    } else {
        cm.write_4bytes(static_cast<uint32_t>(time));
    }
    return *this;
}

MvhdBuilder& MvhdBuilder::set_timescale(uint32_t timescale) {
    builder_.get_chunk_manager().write_4bytes(timescale);
    return *this;
}

MvhdBuilder& MvhdBuilder::set_duration(uint64_t duration) {
    auto& cm = builder_.get_chunk_manager();
    if (version_ == 1) {
        cm.write_8bytes(duration);
    } else {
        cm.write_4bytes(static_cast<uint32_t>(duration));
    }
    return *this;
}

MvhdBuilder& MvhdBuilder::set_rate(uint32_t rate) {
    builder_.get_chunk_manager().write_4bytes(rate);
    return *this;
}

MvhdBuilder& MvhdBuilder::set_volume(uint16_t volume) {
    auto& cm = builder_.get_chunk_manager();
    cm.write_2bytes(0);
    cm.write_8bytes(0);
    builder_.get_chunk_manager().write_2bytes(volume);
    return *this;
}

MvhdBuilder& MvhdBuilder::set_matrix(const std::array<int32_t, 9>& matrix) {
    auto& cm = builder_.get_chunk_manager();
    for (int i = 0; i < 9; ++i) {
        cm.write_4bytes(matrix[i]); // 按 9 个 32-bit 定点数写入
    }
    return *this;
}

MvhdBuilder& MvhdBuilder::set_next_track_id(uint32_t id) {
    builder_.get_chunk_manager().write_4bytes(id);
    return *this;
}