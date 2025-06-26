#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <vector>
#include <leveldb/db.h>

namespace mms {
class Recorder;
class RecorderDb {
public:
    static RecorderDb & get_instance();
    bool init();
    void safe_put(const std::string & key, const std::string & val);
    std::string safe_get(const std::string & key);
    std::vector<std::string> safe_search(const std::string & prefix); 
    virtual ~RecorderDb();
private:
    std::mutex db_mtx_;
    leveldb::DB* db_ = nullptr;
private:
    RecorderDb();
    static RecorderDb instance_;
};
};