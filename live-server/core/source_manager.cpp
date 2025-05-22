/*
 * @Author: jbl19860422
 * @Date: 2023-12-24 14:04:09
 * @LastEditTime: 2023-12-29 18:04:05
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\media_manager.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "source_manager.hpp"

#include "core/media_source.hpp"
#include "spdlog/spdlog.h"
#include "app/publish_app.h"

using namespace mms;
SourceManager SourceManager::instance_;
bool SourceManager::add_source(const std::string & domain, 
                               const std::string & app_name,
                               const std::string & stream_name, 
                               std::shared_ptr<MediaSource> source) {
    std::lock_guard<std::mutex> lck(sources_mtx_);
    auto it = sources_[domain][app_name].find(stream_name);
    if (it != sources_[domain][app_name].end()) {
        return false;
    }
    sources_[domain][app_name][stream_name] = source;
    total_source_count_++;
    map_source_count_[source->get_media_type()]++;
    return true;
}

bool SourceManager::remove_source(const std::string & domain,
                                  const std::string & app_name,
                                  const std::string & stream_name) {
    std::lock_guard<std::mutex> lck(sources_mtx_);
    auto it = sources_[domain][app_name].find(stream_name);
    if (it == sources_[domain][app_name].end()) {
        return false;
    }
    total_source_count_--;
    map_source_count_[it->second->get_media_type()]--;
    sources_[domain][app_name].erase(it);
    return true;
}

std::shared_ptr<MediaSource> SourceManager::get_source(const std::string & domain,
                                                       const std::string & app_name,
                                                       const std::string & stream_name) {
    std::lock_guard<std::mutex> lck(sources_mtx_);
    auto it = sources_[domain][app_name].find(stream_name);
    if (it == sources_[domain][app_name].end()) {
        return nullptr;
    }
    return it->second;
}

std::unordered_map<std::string,
                   std::unordered_map<std::string,
                   std::unordered_map<std::string, 
                   std::shared_ptr<MediaSource>>>> SourceManager::get_sources() {
    std::lock_guard<std::mutex> lck(sources_mtx_);
    return sources_;
}

std::unordered_map<std::string,
                   std::unordered_map<std::string, 
                   std::shared_ptr<MediaSource>>> SourceManager::get_sources(const std::string & domain) {
    std::lock_guard<std::mutex> lck(sources_mtx_);
    return sources_[domain];
}

std::unordered_map<std::string,std::shared_ptr<MediaSource>> SourceManager::get_sources(const std::string & domain,
                                                                                        const std::string & app_name) {
    std::lock_guard<std::mutex> lck(sources_mtx_);;
    return sources_[domain][app_name];
}

void SourceManager::close() {
    std::lock_guard<std::mutex> lck(sources_mtx_);
    for (auto it_domain = sources_.begin(); it_domain != sources_.end(); ++it_domain) {
        for (auto it_app = it_domain->second.begin(); it_app != it_domain->second.end(); ++it_app) {
            for (auto it_stream = it_app->second.begin(); it_stream != it_app->second.end(); ++it_stream) {
                auto source = it_stream->second;
                if (source) {
                    source->close();
                }
            }
            it_app->second.clear();
        }
    }
}