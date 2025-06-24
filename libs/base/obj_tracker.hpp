#pragma once
#include <atomic>

#ifdef DEBUG_BUILD

template <typename T>
class ObjTracker {
public:
    ObjTracker() {
        use_count_.fetch_add(1, std::memory_order_relaxed);
    }

    virtual ~ObjTracker() {
        use_count_.fetch_sub(1, std::memory_order_relaxed);
    }

    static uint64_t get_use_count() {
        return use_count_.load(std::memory_order_relaxed);
    }
private:
    static std::atomic<uint64_t> use_count_;
};

template <typename T>
std::atomic<uint64_t> ObjTracker<T>::use_count_{0};

#else

// Release 模式下为空实现
template <typename T>
class ObjTracker {
public:
    ObjTracker() {}
    virtual ~ObjTracker() {}
    static uint64_t get_use_count() { return 0; }
};

#endif
