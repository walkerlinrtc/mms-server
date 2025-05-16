#include "server_statistic.h"
using namespace mms;
ServerStatistic ServerStatistic::instance_;
ServerStatistic::ServerStatistic() {

}

ServerStatistic::~ServerStatistic() {

}

int64_t ServerStatistic::get_service_time() {
    return time(NULL) - service_start_time_;
}

time_t ServerStatistic::get_start_time() {
    return service_start_time_;
}

void ServerStatistic::inc_playing_user_count() {
    playing_user_count_++;
}

void ServerStatistic::dec_playing_user_count() {
    playing_user_count_--;
}

int64_t ServerStatistic::get_playing_user_count() {
    return playing_user_count_.load();
}

void ServerStatistic::inc_flv_playing_user_count() {
    flv_playing_user_count_++;
}

void ServerStatistic::dec_flv_playing_user_count() {
    flv_playing_user_count_--;
}

int64_t ServerStatistic::get_flv_playing_user_count() {
    return flv_playing_user_count_.load();
}

