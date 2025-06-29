#include <filesystem>
#include "recorder_db.h"
#include "recorder.h"
#include "spdlog/spdlog.h"
#include "base/utils/utils.h"
#include "log/log.h"
using namespace mms;
RecorderDb RecorderDb::instance_;
RecorderDb & RecorderDb::get_instance() {
    return instance_;
}

RecorderDb::RecorderDb() {

}

RecorderDb::~RecorderDb() {
    if (db_) {
        delete db_;
        db_ = nullptr;
    }
}

bool RecorderDb::init() {
    leveldb::Options options;
    options.create_if_missing = true;  // 如果数据库不存在就创建
    // 打开数据库
    std::filesystem::create_directories(Utils::get_bin_path() + "/db");
    auto db_file = Utils::get_bin_path() + "/db/record.db";
    leveldb::Status status = leveldb::DB::Open(options, db_file, &db_);
    if (!status.ok()) {
        leveldb::Status repair_status = leveldb::RepairDB(db_file, options);
        if (repair_status.ok()) {
            status = leveldb::DB::Open(options, db_file, &db_);
            if (!status.ok()) {
                CORE_ERROR("open leveldb for record failed");
                return false;
            }
        }
        return false;
    }

    return true;
}

void RecorderDb::safe_put(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(db_mtx_);
    db_->Put(leveldb::WriteOptions(), key, value);
}

std::string RecorderDb::safe_get(const std::string& key) {
    std::string value;
    db_->Get(leveldb::ReadOptions(), key, &value);
    return value;
}

std::vector<std::string> RecorderDb::safe_search(const std::string& prefix) {
    std::vector<std::string> results;
    leveldb::ReadOptions read_options;
    // 可选：设置 snapshot 保证读取一致性
    read_options.snapshot = db_->GetSnapshot();
    std::unique_ptr<leveldb::Iterator> it(db_->NewIterator(read_options));
    for (it->Seek(prefix); it->Valid(); it->Next()) {
        std::string key = it->key().ToString();
        // 判断是否以 prefix 开头
        if (key.compare(0, prefix.size(), prefix) != 0) {
            break;  // 不再匹配，退出
        }
        results.push_back(it->value().ToString());
    }

    db_->ReleaseSnapshot(read_options.snapshot);
    return results;
}


