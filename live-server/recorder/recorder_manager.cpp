#include "recorder_manager.h"
#include "recorder.h"
#include "spdlog/spdlog.h"
using namespace mms;
RecorderManager RecorderManager::instance_;
RecorderManager & RecorderManager::get_instance() {
    return instance_;
}

RecorderManager::RecorderManager() {

}

RecorderManager::~RecorderManager() {

}

std::unordered_map<std::string,
                    std::unordered_map<std::string,
                    std::unordered_map<std::string, 
                    std::set<std::shared_ptr<Recorder>>>>> RecorderManager::get_recorders() {
    std::lock_guard<std::mutex> lck(recorders_mtx_);
    return recorders_;                      
}

std::unordered_map<std::string,
                    std::unordered_map<std::string, 
                    std::set<std::shared_ptr<Recorder>>>> RecorderManager::get_recorders(const std::string & domain) {
    std::lock_guard<std::mutex> lck(recorders_mtx_);
    return recorders_[domain];
}

std::unordered_map<std::string,
                    std::set<std::shared_ptr<Recorder>>> RecorderManager::get_recorders(const std::string & domain, const std::string & app_name) {
    std::lock_guard<std::mutex> lck(recorders_mtx_);
    return recorders_[domain][app_name];
}

std::set<std::shared_ptr<Recorder>> RecorderManager::get_recorder(const std::string & domain,
                                                                const std::string & app_name,
                                                                const std::string & stream_name) {
    std::lock_guard<std::mutex> lck(recorders_mtx_);
    auto it = recorders_[domain][app_name].find(stream_name);
    if (it == recorders_[domain][app_name].end()) {
        return {};
    }
    return it->second;
}

void RecorderManager::add_recorder(std::shared_ptr<Recorder> r) {
    std::lock_guard<std::mutex> lck(recorders_mtx_);
    recorders_[r->get_domain_name()][r->get_app_name()][r->get_stream_name()].insert(r);
}

void RecorderManager::remove_recorder(std::shared_ptr<Recorder> r) {
    std::lock_guard<std::mutex> lck(recorders_mtx_);
    auto it_domain = recorders_.find(r->get_domain_name());
    if (it_domain == recorders_.end()) {
        return;
    }

    auto it_app = it_domain->second.find(r->get_app_name());
    if (it_app == it_domain->second.end()) {
        return;
    }

    auto it_stream = it_app->second.find(r->get_stream_name());
    if (it_stream == it_app->second.end()) {
        return;
    }

    auto & record_set = it_stream->second;
    for (auto it_record = record_set.begin(); it_record != record_set.end(); it_record++) {
        if ((*it_record) == r) {
            record_set.erase(it_record);
            break;
        }
    }
}

