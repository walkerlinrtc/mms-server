#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <set>

namespace mms {
class Recorder;
class RecorderManager {
public:
    static RecorderManager & get_instance();
    void add_recorder(std::shared_ptr<Recorder> r);
    void remove_recorder(std::shared_ptr<Recorder> r);
    std::unordered_map<std::string,
                       std::unordered_map<std::string,
                       std::unordered_map<std::string, 
                       std::set<std::shared_ptr<Recorder>>>>> get_recorders();

    std::unordered_map<std::string,
                       std::unordered_map<std::string, 
                       std::set<std::shared_ptr<Recorder>>>> get_recorders(const std::string & domain);
    
    std::unordered_map<std::string,
                       std::set<std::shared_ptr<Recorder>>> get_recorders(const std::string & domain, const std::string & app_name);
    
    std::set<std::shared_ptr<Recorder>> get_recorder(const std::string & domain,
                                            const std::string & app_name,
                                            const std::string & stream_name);
    virtual ~RecorderManager();
private:
    std::mutex recorders_mtx_;
    std::unordered_map<std::string,
                       std::unordered_map<std::string,
                       std::unordered_map<std::string, 
                       std::set<std::shared_ptr<Recorder>>>>> recorders_;
private:
    RecorderManager();
    static RecorderManager instance_;
};
};