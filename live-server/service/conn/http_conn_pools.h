#pragma once
#include <unordered_map>
#include <memory>
#include <mutex>

#include "base/network/conn_pool.hpp"
#include "sdk/http/http_long_client.hpp"

namespace mms {
class ThreadWorker;

class HttpConnPools {
public:
    HttpConnPools();
    virtual ~HttpConnPools();
    static HttpConnPools & get_instance() {
        return instance_;
    }

    void set_worker(ThreadWorker *worker) {
        worker_ = worker;
    }
    
    std::shared_ptr<ConnPool<HttpLongClient>> get_http_conn_pool(const std::string & domain, uint16_t port);
    void create_pool(const std::string & domain, uint16_t port, int min_count, int max_count, const std::function<boost::asio::awaitable<std::shared_ptr<HttpLongClient>>()> & allocator);
private:
    ThreadWorker *worker_ = nullptr;
    std::mutex http_conn_pools_mtx_;
    std::unordered_map<std::string, std::shared_ptr<ConnPool<HttpLongClient>>> http_conn_pools_;
    static HttpConnPools instance_;
};
};