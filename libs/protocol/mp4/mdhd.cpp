#include "mdhd.h"
#include "base/net_buffer.h"
using namespace mms;

MdhdBox::MdhdBox() : FullBox(BOX_TYPE_MDHD, 0, 0) {
    creation_time_ = 0;
    modification_time_ = 0;
    timescale_ = 1000; // 常见默认时间尺度
    duration_ = 0;
    language_ = 0x55C4; // 对应"und"未知语言
    pre_defined_ = 0;
}

MdhdBox::~MdhdBox() {

}

char MdhdBox::language0()
{
    return (char)(((language_ >> 10) & 0x1f) + 0x60);
}

void MdhdBox::set_language0(char v)
{
    language_ |= uint16_t((uint8_t(v) - 0x60) & 0x1f) << 10;
}

char MdhdBox::language1()
{
    return (char)(((language_ >> 5) & 0x1f) + 0x60);
}

void MdhdBox::set_language1(char v)
{
    language_ |= uint16_t((uint8_t(v) - 0x60) & 0x1f) << 5;
}

char MdhdBox::language2()
{
    return (char)((language_ & 0x1f) + 0x60);
}

void MdhdBox::set_language2(char v)
{
    language_ |= uint16_t((uint8_t(v) - 0x60) & 0x1f);
}
#include "spdlog/spdlog.h"
int64_t MdhdBox::size() {
    int64_t total_bytes = FullBox::size();
    // 根据版本计算字段长度
    if (version_ == 1) {
        total_bytes += 8 * 3 + 4; // creation_time(8) + modification_time(8) + duration(8) + timescale(4)
    } else {
        total_bytes += 4 * 3 + 4; // 各32位字段
    }
    total_bytes += 4; // language(2) + pre_defined(2)
    return total_bytes;
}

int64_t MdhdBox::encode(NetBuffer& buf) {
    update_size();
    auto start = buf.pos();
    FullBox::encode(buf); // 编码父类头信息
    
    // 根据版本编码不同长度字段
    if (version_ == 1) {
        buf.write_8bytes(creation_time_);
        buf.write_8bytes(modification_time_);
        buf.write_4bytes(timescale_);
        buf.write_8bytes(duration_);
    } else {
        buf.write_4bytes(static_cast<uint32_t>(creation_time_));
        buf.write_4bytes(static_cast<uint32_t>(modification_time_));
        buf.write_4bytes(timescale_);
        buf.write_4bytes(static_cast<uint32_t>(duration_));
    }
    
    buf.write_2bytes(language_);
    buf.write_2bytes(pre_defined_);
    return buf.pos() - start;
}

int64_t MdhdBox::decode(NetBuffer& buf) {
    auto start = buf.pos();
    FullBox::decode(buf); // 解码父类头信息
    
    // 根据版本读取不同长度字段
    if (version_ == 1) {
        creation_time_ = buf.read_8bytes();
        modification_time_ = buf.read_8bytes();
        timescale_ = buf.read_4bytes();
        duration_ = buf.read_8bytes();
    } else {
        creation_time_ = buf.read_4bytes();
        modification_time_ = buf.read_4bytes();
        timescale_ = buf.read_4bytes();
        duration_ = buf.read_4bytes();
    }
    
    language_ = buf.read_2bytes();
    pre_defined_ = buf.read_2bytes();
    
    return buf.pos() - start;
}