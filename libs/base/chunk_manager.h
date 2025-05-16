#pragma once
#include <stdint.h>
#include <list>
#include <boost/asio/buffer.hpp>
#include "chunk.h"

namespace mms {
class ChunkManager {
public:
    // 默认每个 chunk 大小为 1MB（可自定义）
    static constexpr size_t DEFAULT_CHUNK_SIZE = 1 * 1024 * 1024;

    explicit ChunkManager(size_t chunk_size = DEFAULT_CHUNK_SIZE)
        : chunk_size_(chunk_size > 0 ? chunk_size : DEFAULT_CHUNK_SIZE) {
        add_chunk(); // 初始化首个 chunk
    }

    // 获取总数据大小（含所有 chunk 和引用数据）
    size_t total_size() const {
        size_t size = 0;
        for (const auto& chunk : chunks_) {
            size += chunk->get_used_bytes();
        }
        return size;
    }

    // 将所有数据合并到连续内存（可选操作）
    std::vector<boost::asio::const_buffer> get_raw_bufs() const;
    // 访问所有 chunk（用于流式处理）
    const std::list<std::shared_ptr<Chunk>> & chunks() const { 
        return chunks_; 
    }
    // 获取当前分块
    std::shared_ptr<Chunk> get_curr_chunk() {
        return curr_chunk_;
    }

    // 操作函数
    void skip(size_t bytes);
    void write_1bytes(uint8_t v);
    void write_2bytes(uint16_t v);
    void write_3bytes(uint32_t v);
    void write_4bytes(uint32_t v);
    void write_8bytes(uint64_t v);
    void write_string(const std::string & v);
    void write_string(const std::string_view & v);
    void write_string(const char *data, size_t len);
private:
    void add_chunk();
    // 成员变量
    size_t chunk_size_;         // 每个 chunk 的容量
    std::shared_ptr<Chunk> curr_chunk_;
    std::list<std::shared_ptr<Chunk>> chunks_;   // 内存块列表
};

};