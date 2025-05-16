#include "stco.h"
#include "base/net_buffer.h"
using namespace mms;

StcoBox::StcoBox()
    : FullBox(BOX_TYPE_STCO, 0, 0)  // stco盒子版本固定为0
{
}

StcoBox::~StcoBox() = default;

int64_t StcoBox::size() {
    // FullBox头(12字节) + entry_count(4字节) + entries(每个4字节)
    return FullBox::size() + 4 + entries.size() * 4;
}

int64_t StcoBox::encode(NetBuffer& buf) {
    update_size();
    auto start = buf.pos();
    
    // 编码父类头信息
    FullBox::encode(buf);
    
    // 写入条目数量及所有条目
    buf.write_4bytes(static_cast<uint32_t>(entries.size()));
    for (auto & e : entries) {
        buf.write_4bytes(e);
    }
    
    return buf.pos() - start;
}

int64_t StcoBox::decode(NetBuffer& buf) {
    auto start = buf.pos();
    
    // 解码父类头信息
    FullBox::decode(buf);
    
    // 验证版本必须为0
    if (version_ != 0) {
        // 可抛出异常或记录错误日志
        return -1; 
    }
    
    // 读取条目数量
    uint32_t entry_count = buf.read_4bytes();
    
    // 读取所有chunk offset条目
    entries.resize(entry_count);
    for (uint32_t i = 0; i < entry_count; ++i) {
        entries[i] = buf.read_4bytes();
    }
    
    return buf.pos() - start;
}