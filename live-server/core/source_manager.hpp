#pragma once
#include <unordered_map>
#include <mutex>
#include <memory>
#include <string>
#include <atomic>

#include <boost/serialization/singleton.hpp> 
namespace mms {
class MediaSource;
class SourceManager {
public:
    static SourceManager & get_instance() {
        return instance_;
    }

    bool add_source(const std::string & domain, 
                    const std::string & app_name,
                    const std::string & stream_name, std::shared_ptr<MediaSource> source);

    bool remove_source(const std::string & domain, 
                       const std::string & app_name,
                       const std::string & stream_name);
                    
    std::shared_ptr<MediaSource> get_source(const std::string & domain,
                                            const std::string & app_name,
                                            const std::string & stream_name);

    std::unordered_map<std::string,
                       std::unordered_map<std::string,
                       std::unordered_map<std::string, 
                       std::shared_ptr<MediaSource>>>> get_sources();

    std::unordered_map<std::string,
                       std::unordered_map<std::string, 
                       std::shared_ptr<MediaSource>>> get_sources(const std::string & domain);
    
    std::unordered_map<std::string,
                       std::shared_ptr<MediaSource>> get_sources(const std::string & domain,
                                                                 const std::string & app_name);
    void close();
private:
    static SourceManager instance_;
    std::mutex sources_mtx_;
    std::unordered_map<std::string,
                       std::unordered_map<std::string,
                       std::unordered_map<std::string, 
                       std::shared_ptr<MediaSource>>>> sources_;

    std::atomic<int64_t> total_source_count_{0};
    std::unordered_map<std::string, std::atomic<int64_t>> map_source_count_;
};

};