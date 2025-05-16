#pragma once
#include <time.h>
#include <stdint.h>
#include <atomic>

namespace mms {
class ServerStatistic {
public:
    ServerStatistic();
    virtual ~ServerStatistic();
public:
    static ServerStatistic & get_instance() {
        return instance_;
    }

    int64_t get_service_time();
    time_t get_start_time();

    void inc_playing_user_count();
    void dec_playing_user_count();
    int64_t get_playing_user_count();

    void inc_flv_playing_user_count();
    void dec_flv_playing_user_count();
    int64_t get_flv_playing_user_count();
private:
    time_t service_start_time_ = time(NULL);
    std::atomic<int64_t> playing_user_count_{0};
    std::atomic<int64_t> flv_playing_user_count_{0};
private:
    static ServerStatistic instance_;
};
};