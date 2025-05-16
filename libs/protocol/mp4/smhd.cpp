#include "smhd.h"
#include "base/net_buffer.h"
using namespace mms;

SmhdBox::SmhdBox() 
    : FullBox(BOX_TYPE_SMHD, 0, 0)  // version和flags初始化为0
{
    balance = 0;        // 默认居中
    reserved = 0;       // 保留字段必须置0
}

SmhdBox::~SmhdBox() = default;

int64_t SmhdBox::size() {
    // FullBox头(12字节) + balance(2) + reserved(2) = 16字节
    return FullBox::size() + sizeof(balance) + sizeof(reserved);
}

int64_t SmhdBox::encode(NetBuffer& buf) {
    auto start = buf.pos();
    
    // 编码父类头信息
    FullBox::encode(buf);
    
    // 写入音频头特有字段
    buf.write_2bytes(balance);
    buf.write_2bytes(reserved);
    
    return buf.pos() - start;
}

int64_t SmhdBox::decode(NetBuffer& buf) {
    auto start = buf.pos();
    
    // 解码父类头信息
    FullBox::decode(buf);
    
    // 读取音频头特有字段
    balance = buf.read_2bytes();
    reserved = buf.read_2bytes();
    
    return buf.pos() - start;
}