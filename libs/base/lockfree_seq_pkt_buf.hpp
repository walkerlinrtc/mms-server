#pragma once
#include <atomic>
#include <shared_mutex>
#include <vector>

namespace mms {
// 线程安全说明：
// - 写入操作（add_pkt）仅限单线程调用；
// - 多线程可安全读取（get_pkt）；
// - clear() 不可与其他操作并发调用。
template<typename T>
class LockFreeSeqPktBuf {
    using Slot = std::pair<uint64_t, std::shared_ptr<T>>;
    using Buffer = std::vector<Slot>;
    using BufferPtr = std::shared_ptr<Buffer>;

public:
    LockFreeSeqPktBuf(size_t max_size)
        : max_size_(max_size) {
        auto buf0 = std::make_shared<Buffer>();
        buf0->resize(max_size_);
        auto buf1 = std::make_shared<Buffer>();
        buf1->resize(max_size_);

        writing_pkt_buf_.store(buf0);
        free_pkt_buf_.store(buf1);
    }

    ~LockFreeSeqPktBuf() = default;

    uint64_t add_pkt(std::shared_ptr<T> pkt) {
        uint64_t index = next_index_.fetch_add(1, std::memory_order_relaxed);
        auto pkt_buf = writing_pkt_buf_.load(std::memory_order_acquire);
        (*pkt_buf)[index % max_size_] = std::make_pair(index, pkt);
        return index;
    }

    std::shared_ptr<T> get_pkt(uint64_t index) {
        uint64_t latest = next_index_.load(std::memory_order_relaxed);
        if (index >= latest) {
            return nullptr;
        }
        auto pkt_buf = writing_pkt_buf_.load(std::memory_order_acquire);
        const Slot& slot = (*pkt_buf)[index % max_size_];
        if (slot.first != index) {
            return nullptr;  // 已被覆盖
        }
        return slot.second;
    }

    uint64_t latest_index() {
        return next_index_.load(std::memory_order_relaxed);
    }

    void clear() {
        auto old_buf = writing_pkt_buf_.load(std::memory_order_acquire);
        auto new_buf = free_pkt_buf_.load(std::memory_order_acquire);

        // 清空新的缓冲区
        for (auto& slot : *new_buf) {
            slot.first = 0;
            slot.second = nullptr;
        }

        // 交换
        writing_pkt_buf_.store(new_buf, std::memory_order_release);
        free_pkt_buf_.store(old_buf, std::memory_order_release);
    }

private:
    size_t max_size_;
    std::atomic<BufferPtr> writing_pkt_buf_;
    std::atomic<BufferPtr> free_pkt_buf_;
    std::atomic<uint64_t> next_index_{0};
};

};