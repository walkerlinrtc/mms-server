#include "mp4_builder.h"
#include "ftyp.h"
#include "moov.h"

using namespace mms;

Mp4Builder::Mp4Builder() {

}

Mp4Builder::~Mp4Builder() {

}

// 开始一个 Box
void Mp4Builder::begin_box(Box::Type type) {
    // 预留 8 字节头部（size + type）
    auto chunk = chunk_manager_.get_curr_chunk();
    box_stack_.push({chunk, chunk->pos()});
    chunk_manager_.write_4bytes(0);
    chunk_manager_.write_4bytes(type);
}

// 结束当前 Box，回填实际大小
void Mp4Builder::end_box() {
    auto s = box_stack_.top();
    box_stack_.pop();

    // 计算 Box 内容大小（包括头部）
    size_t box_size = chunk_manager_.total_size();  // size 字段自身占 4 字节
    // 回填 size 到起始位置
    uint8_t* ptr = (uint8_t*)(s.size_chunk_ptr_->get_raw_buf().data() + s.size_chunk_offset_);
    ptr[0] = (box_size >> 24) & 0xFF;
    ptr[1] = (box_size >> 16) & 0xFF;
    ptr[2] = (box_size >> 8) & 0xFF;
    ptr[3] = box_size & 0xFF;
}

// 添加 ftyp Box
FtypBuilder Mp4Builder::add_ftyp() {
    return FtypBuilder(*this);
}

MoovBuilder Mp4Builder::add_moov() {
    return MoovBuilder(*this);
}