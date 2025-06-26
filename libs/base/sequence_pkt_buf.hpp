#pragma once
#include <atomic>
#include <shared_mutex>
#include <vector>
#include <memory>
#include <mutex>

namespace mms {
template<typename T>
class SequencePktBuf {
public:
    SequencePktBuf(size_t max_size) : indexes_(0) {
        pkt_buf_.resize(max_size);
        max_size_ = max_size;
    }

    virtual ~SequencePktBuf() {

    }

    uint64_t add_pkt(std::shared_ptr<T> pkt) {
        std::unique_lock<std::shared_mutex> wlock(rw_mutex_);
        indexes_++;
        pkt_buf_[indexes_%max_size_] = std::make_pair(indexes_, pkt);
        return indexes_;
    }

    std::shared_ptr<T> get_pkt(uint64_t index) {
        std::shared_lock<std::shared_mutex> rlock(rw_mutex_);
        return pkt_buf_[index%max_size_].second;
    }

    uint64_t latest_index() {
        return indexes_;
    }

    std::vector<std::shared_ptr<T>> dump_pkts() {
        std::vector<std::shared_ptr<T>> v;
        std::unique_lock<std::shared_mutex> wlock(rw_mutex_);
        int start_idx = indexes_%max_size_;
        uint64_t buf_index = indexes_;
        int curr_idx = start_idx;
        do {
            auto pkt = pkt_buf_[curr_idx];
            if (!pkt.second) {
                break;
            }
            v.push_back(pkt.second);
            buf_index--;
            if (buf_index <= 0) {
                break;
            }
            curr_idx = buf_index%max_size_;
        } while (curr_idx != start_idx);
        return v;
    }

    void clear() {
        std::unique_lock<std::shared_mutex> wlock(rw_mutex_);
        uint64_t current_index = indexes_;
        size_t idx = current_index % max_size_;
        while (pkt_buf_[idx].second != nullptr) {
            pkt_buf_[idx].second = nullptr;
            current_index--;
            idx = current_index % max_size_;
        }
    }
private:
    size_t max_size_;
    std::shared_mutex rw_mutex_;
    std::vector<std::pair<uint64_t, std::shared_ptr<T>>> pkt_buf_;
    uint64_t indexes_;
};

};