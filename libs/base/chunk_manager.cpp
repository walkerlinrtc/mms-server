#include "chunk_manager.h"
using namespace mms;

// 动态添加新 chunk
void ChunkManager::add_chunk() {
    auto chunk = std::make_shared<Chunk>(chunk_size_);
    chunks_.emplace_back(chunk);
    curr_chunk_ = chunk;
}

std::vector<boost::asio::const_buffer> ChunkManager::get_raw_bufs() const {
    std::vector<boost::asio::const_buffer> bufs;
    bufs.reserve(chunks_.size());
    // 拷贝 chunk 数据
    for (const auto& chunk : chunks_) {
        bufs.push_back(boost::asio::const_buffer(chunk->get_remaining_buf().data(), chunk->get_remaining_buf().size()));
    }
    return bufs;
}

void ChunkManager::skip(size_t bytes) {
    if (curr_chunk_->get_remaining_bytes() < bytes) {
        add_chunk();
    }
    curr_chunk_->skip(bytes);
}

void ChunkManager::write_1bytes(uint8_t v) {
    if (curr_chunk_->get_remaining_bytes() < 1) {
        add_chunk();
    }
    curr_chunk_->write_1bytes(v);
}

void ChunkManager::write_2bytes(uint16_t v) {
    if (curr_chunk_->get_remaining_bytes() < 2) {
        add_chunk();
    }
    curr_chunk_->write_1bytes(v);
}

void ChunkManager::write_3bytes(uint32_t v) {
    if (curr_chunk_->get_remaining_bytes() < 3) {
        add_chunk();
    }
    curr_chunk_->write_3bytes(v);
}

void ChunkManager::write_4bytes(uint32_t v) {
    if (curr_chunk_->get_remaining_bytes() < 4) {
        add_chunk();
    }
    curr_chunk_->write_4bytes(v);
}

void ChunkManager::write_8bytes(uint64_t v) {
    if (curr_chunk_->get_remaining_bytes() < 8) {
        add_chunk();
    }
    curr_chunk_->write_8bytes(v);
}

void ChunkManager::write_string(const std::string & v) {
    if (curr_chunk_->get_remaining_bytes() < v.size()) {
        add_chunk();
    }
    curr_chunk_->write_string(v);
}

void ChunkManager::write_string(const std::string_view & v) {
    if (curr_chunk_->get_remaining_bytes() < v.size()) {
        add_chunk();
    }
    curr_chunk_->write_string(v);
}

void ChunkManager::write_string(const char *data, size_t len) {
    if (curr_chunk_->get_remaining_bytes() < len) {
        add_chunk();
    }
    curr_chunk_->write_string(data, len);
}