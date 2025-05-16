#pragma once
#include <string_view>
#include "full_box.h"

namespace mms {
class NetBuffer;
class Mp4Builder;

class MvhdBox : public FullBox {
public:
    MvhdBox();
    virtual ~MvhdBox() = default;
    int64_t size() override;
    int64_t encode(NetBuffer & buf) override;
    int64_t decode(NetBuffer & buf) override;
public:
    uint64_t creation_time_; // seconds sine midnight, Jan. 1, 1904, UTC
	uint64_t modification_time_; // seconds sine midnight, Jan. 1, 1904, UTC
    uint32_t timescale_; // time-scale for the entire presentation, the number of time units that pass in one second
	uint64_t duration_; // default UINT64_MAX(by timescale)
	
	uint32_t rate_;
	uint16_t volume_; // fixed point 8.8 number, 1.0 (0x0100) is full volume
	//uint16_t reserved;
	//uint32_t reserved2[2];
	int32_t matrix_[9] = {0x00010000,0,0,0,0x00010000,0,0,0,0x40000000}; // u,v,w
	//int32_t pre_defined[6];
	uint32_t next_track_ID_;
};

class MvhdBuilder {
public:
    MvhdBuilder(Mp4Builder & builder);
    
    // 核心字段设置（链式调用）
    MvhdBuilder& set_version(uint8_t version);          // 0 或 1
    MvhdBuilder& set_flags(uint32_t flags);             // 通常为 0
    MvhdBuilder& set_creation_time(uint64_t time);      // 根据版本自动选择 4/8 字节
    MvhdBuilder& set_modification_time(uint64_t time);  // 同上
    MvhdBuilder& set_timescale(uint32_t timescale);     // 时间单位/秒（如 1000=毫秒）
    MvhdBuilder& set_duration(uint64_t duration);       // 总时长（timescale 单位）
    MvhdBuilder& set_rate(uint32_t rate);               // 播放速率（1.0 = 0x00010000）
    MvhdBuilder& set_volume(uint16_t volume);           // 音量（1.0 = 0x0100）
    MvhdBuilder& set_matrix(const std::array<int32_t, 9>& matrix); // 视频变换矩阵
    MvhdBuilder& set_next_track_id(uint32_t id);        // 下一个轨道ID

    virtual ~MvhdBuilder();

private:
    Mp4Builder & builder_;
    uint8_t version_ = 0; // 默认版本 0
};

    
};