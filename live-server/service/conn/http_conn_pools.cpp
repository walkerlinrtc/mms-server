/*
 * @Author: jbl19860422
 * @Date: 2023-12-26 22:24:33
 * @LastEditTime: 2023-12-27 21:11:09
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\conn_manager\http_conn_pools.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include "http_conn_pools.h"
using namespace mms;
HttpConnPools HttpConnPools::instance_;

HttpConnPools::HttpConnPools() {
    
}

HttpConnPools::~HttpConnPools() {

}

void HttpConnPools::create_pool(const std::string & domain, uint16_t port, int min_count, int max_count, const std::function<boost::asio::awaitable<std::shared_ptr<HttpLongClient>>()> & allocator) {
    std::string key = domain + ":" + std::to_string(port);
    {
        std::lock_guard<std::mutex> lck(http_conn_pools_mtx_);
        auto it = http_conn_pools_.find(key);
        if (it != http_conn_pools_.end()) {
            return;
        }
    }
    
    boost::asio::co_spawn(worker_->get_io_context(), [this, key, min_count, max_count, allocator]() -> boost::asio::awaitable<void> {
        auto http_conn_pool = std::make_shared<ConnPool<HttpLongClient>>();
        bool ret = co_await http_conn_pool->init(min_count, max_count, allocator);
        if (!ret) {
            co_return;
        }

        {
            std::lock_guard<std::mutex> lck(http_conn_pools_mtx_);
            http_conn_pools_[key] = http_conn_pool;
        }
        co_return;
    }, boost::asio::detached);
}

std::shared_ptr<ConnPool<HttpLongClient>> HttpConnPools::get_http_conn_pool(const std::string & domain, uint16_t port) {
    std::string key = domain + ":" + std::to_string(port);
     std::lock_guard<std::mutex> lck(http_conn_pools_mtx_);
     auto it = http_conn_pools_.find(key);
     if (it == http_conn_pools_.end()) {
        return nullptr;
     }
     return it->second;
}