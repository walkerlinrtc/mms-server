#include "tkhd.h"
#include "mp4_factory.h"
#include <string.h>

using namespace mms;
int64_t TkhdBox::size() {
    int64_t total_bytes = FullBox::size();
    if (version_ == 1) {
        total_bytes += 32;//creation_time(8) + modification_time(8) + track_ID(4) + reverved(4) + duration(8)
    } else {
        total_bytes += 20;//4*5
    }
    total_bytes += 8;//reverved(8)
    total_bytes += 8;//layer(2)+alternate_group(2)+volume(2)+reserved(2)
    total_bytes += 4*9;//matrix[9]
    total_bytes += 8;//width(4)+height(4)
    return total_bytes;
}

int64_t TkhdBox::encode(NetBuffer& buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf);

    // 时间相关字段
    if (version_ == 1) {
        buf.write_8bytes(creation_time_);
        buf.write_8bytes(modification_time_);
    } else {
        buf.write_4bytes(static_cast<uint32_t>(creation_time_));
        buf.write_4bytes(static_cast<uint32_t>(modification_time_));
    }

    // track元信息
    buf.write_4bytes(track_ID_);
    buf.write_4bytes(0); // reserved1

    // 持续时间
    if (version_ == 1) {
        buf.write_8bytes(duration_);
    } else {
        buf.write_4bytes(static_cast<uint32_t>(duration_));
    }

    // 保留字段
    buf.write_8bytes(0); // reserved2

    // 轨道参数
    buf.write_2bytes(layer_);
    buf.write_2bytes(alternate_group_);
    buf.write_2bytes(volume_);
    buf.write_2bytes(0); // reserved3

    // 矩阵写入
    for (int i = 0; i < 9; ++i) {
        buf.write_4bytes(matrix_[i]);
    }

    // 分辨率
    buf.write_4bytes(width_);
    buf.write_4bytes(height_);

    return buf.pos() - start;
}

int64_t TkhdBox::decode(NetBuffer& buf) {
    auto start = buf.pos();
    FullBox::decode(buf);

    // 版本校验
    if (version_ > 1) {
        throw std::runtime_error("Invalid tkhd version");
    }

    // 时间解析
    if (version_ == 1) {
        creation_time_ = buf.read_8bytes();
        modification_time_ = buf.read_8bytes();
    } else {
        creation_time_ = buf.read_4bytes();
        modification_time_ = buf.read_4bytes();
    }

    // track元信息
    track_ID_ = buf.read_4bytes();
    buf.skip(4);

    // 持续时间
    if (version_ == 1) {
        duration_ = buf.read_8bytes();
    } else {
        duration_ = buf.read_4bytes();
    }

    // 保留字段校验
    buf.skip(8);

    // 轨道参数
    layer_ = buf.read_2bytes();
    alternate_group_ = buf.read_2bytes();
    volume_ = buf.read_2bytes();
    buf.skip(2);
    // 矩阵读取
    for (int i = 0; i < 9; ++i) {
        matrix_[i] = buf.read_4bytes();
    }

    // 分辨率
    width_ = buf.read_4bytes();
    height_ = buf.read_4bytes();

    return buf.pos() - start;
}