/*
 * @Author: jbl19860422
 * @Date: 2022-12-23 23:36:16
 * @LastEditTime: 2023-08-27 09:59:49
 * @LastEditors: jbl19860422
 * @Description:
 * @FilePath: \mms\mms\dns\dns_service.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved.
 */
#include "dns_service.hpp"

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/lexical_cast.hpp>

#include "log/log.h"

using namespace mms;
DnsService DnsService::instance_;
DnsService& DnsService::get_instance() { return instance_; }

DnsService::DnsService() {}

DnsService::~DnsService() {}

void DnsService::start() {
    worker_.start();
    refresh_timer_ = std::make_unique<boost::asio::deadline_timer>(worker_.get_io_context());
    boost::asio::co_spawn(
        worker_.get_io_context(),
        [this]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            while (1) {
                refresh_timer_->expires_from_now(boost::posix_time::milliseconds(resolve_interval_ms_));
                co_await refresh_timer_->async_wait(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (boost::asio::error::operation_aborted == ec) {
                    break;
                }

                std::lock_guard<std::recursive_mutex> lck(lock_);
                for (auto& pair : domain_ips_) {
                    const std::string& domain = pair.first;
                    refresh(domain);
                }
            }
            co_return;
        },
        [this](std::exception_ptr exp) { ((void)exp); });
}

void DnsService::stop() {
    refresh_timer_->cancel();
    worker_.stop();
}

void DnsService::add_domain(const std::string& domain) {
    bool new_add = false;
    {
        std::lock_guard<std::recursive_mutex> lck(lock_);
        auto it = domain_ips_.find(domain);
        if (it == domain_ips_.end()) {
            domain_ips_[domain];
            new_add = true;
        }
    }

    if (new_add) {
        CORE_INFO("add dns domain:{}", domain);
        refresh(domain);
    }
}

void DnsService::remove_domain(const std::string& domain) {
    std::lock_guard<std::recursive_mutex> lck(lock_);
    domain_ips_.erase(domain);
}

std::optional<std::string> DnsService::get_ip(const std::string& domain) {
    std::lock_guard<std::recursive_mutex> lck(lock_);
    auto it = domain_ips_.find(domain);
    if (it == domain_ips_.end() || it->second.size() <= 0) {
        add_domain(domain);
        auto ip = resolve_domain_now(domain);
        return ip;
    }

    srand(time(0));
    int index = rand() % (it->second.size());
    return it->second[index];
}

std::vector<std::string> DnsService::get_ips(const std::string& domain) {
    std::lock_guard<std::recursive_mutex> lck(lock_);
    auto it = domain_ips_.find(domain);
    if (it == domain_ips_.end()) {
        return {};
    }
    return it->second;
}

void DnsService::set_resolve_interval(int ms) { resolve_interval_ms_ = ms; }

void DnsService::refresh(const std::string& domain) {
    boost::asio::co_spawn(
        worker_.get_io_context(),
        [this, domain]() -> boost::asio::awaitable<void> {
            boost::system::error_code ec;
            boost::asio::ip::tcp::resolver slv(worker_.get_io_context());
            auto results = co_await slv.async_resolve(
                domain, "0", boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) {
                co_return;
            }

            if (results.empty()) {
                co_return;
            }

            {
                std::lock_guard<std::recursive_mutex> lck(lock_);
                auto it_domain = domain_ips_.find(domain);
                if (it_domain == domain_ips_.end()) {
                    co_return;
                }

                auto& ips = it_domain->second;
                ips.clear();
                for (const auto& result : results) {
                    ips.push_back(result.endpoint().address().to_string());
                }
            }
            co_return;
        },
        boost::asio::detached);
}

std::optional<std::string> DnsService::resolve_domain_now(const std::string& domain) {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::resolver rslv(io_context);
    auto results = rslv.resolve(domain, "0");
    if (results.empty()) {
        return std::nullopt;
    }
    return results.begin()->endpoint().address().to_string();
}