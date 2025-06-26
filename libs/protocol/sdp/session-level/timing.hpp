#pragma once
#include <string>
#include <cstdint>
namespace mms {
struct Timing {
public:
    static std::string prefix;
    Timing() = default;
    Timing(uint64_t start_time, uint64_t stop_time) {
        start_time_ = start_time;
        stop_time_ = stop_time;
    }

    bool parse(const std::string & line);

    uint64_t get_start_time() const {
        return start_time_;
    }

    void set_start_time(uint64_t val) {
        start_time_ = val;
    }

    uint64_t get_stop_time() const {
        return stop_time_;
    }

    void set_stop_time(uint64_t val) {
        stop_time_ = val;
    }

    std::string to_string() const;
private:
    uint64_t start_time_ = 0;
    uint64_t stop_time_ = 0;
};
};