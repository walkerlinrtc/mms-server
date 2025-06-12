#pragma once
#include "spdlog/spdlog.h"

#include <memory>
#include <string>
#include <atomic>
#include <unordered_map>
#include <optional>

#include <boost/asio/awaitable.hpp>

#include "json/json.h"
namespace mms {
class App;
class ThreadWorker;

class Session : public std::enable_shared_from_this<Session> {
    friend class App;
public:
    Session(ThreadWorker *worker);
    virtual ~Session();
    virtual void service() = 0;
    virtual void close() = 0;
    
    ThreadWorker *get_worker() const {
        return worker_;
    }

    std::string & get_session_name() {
        return session_name_;
    }

    void set_session_type(const std::string & session_type) {
        session_type_ = session_type;
    }

    const std::string & get_session_type() {
        return session_type_;
    }

    void set_param(const std::string & key, const std::string & val) {
        params_[key] = val;
    }

    std::optional<std::reference_wrapper<const std::string>> get_param(const std::string & key);

    void set_header(const std::string & key, const std::string & val) {
        headers_[key] = val;
    }

    std::optional<std::reference_wrapper<const std::string>> get_header(const std::string & key);

    virtual Json::Value to_json();
protected:
    std::atomic_flag closed_ = ATOMIC_FLAG_INIT;
    uint64_t id_;
    std::string session_name_;
    std::string session_type_;
    std::unordered_map<std::string, std::string> params_;
    std::unordered_map<std::string, std::string> headers_;
    ThreadWorker *worker_ = nullptr;
private:
    static std::atomic<uint64_t> session_id_;
};

};