#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <optional>

#include "base/thread/thread_worker.hpp"

namespace mms {
class DnsService {
public:
    static DnsService & get_instance();
    virtual ~DnsService();
    void start();
    void stop();
    
    void set_resolve_interval(int ms);
    /*
    * @func:添加到dns解析列表，并立即开始解析
    */
    void add_domain(const std::string & domain);
    void remove_domain(const std::string & domain);
    std::optional<std::string> get_ip(const std::string & domain);
    std::vector<std::string> get_ips(const std::string & domain);
    std::optional<std::string> resolve_domain_now(const std::string & domain);
private:
    void refresh(const std::string & domain);
    std::unique_ptr<boost::asio::deadline_timer> refresh_timer_;
    std::recursive_mutex lock_;
    std::unordered_map<std::string, std::vector<std::string>> domain_ips_;
    ThreadWorker worker_;
    int resolve_interval_ms_ = 60000;//默认1分钟一次
private:
    DnsService();
    static DnsService instance_;
};
};