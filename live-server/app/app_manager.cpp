#include "app.h"
#include "app_manager.h"

using namespace mms;
AppManager AppManager::instance_;

AppManager::AppManager() {

}

AppManager::~AppManager() {

}

std::shared_ptr<App> AppManager::get_app(const std::string & domain, const std::string & app_name) {
    std::shared_lock<std::shared_mutex> lck(mutex_);
    auto it_domain = domain_apps_.find(domain);
    if (it_domain == domain_apps_.end()) {
        return nullptr;
    }

    auto it_app = it_domain->second.find(app_name);
    if (it_app == it_domain->second.end()) {
        return nullptr;
    }
    return it_app->second;
}

void AppManager::add_app(const std::string & domain, const std::string & app_name, std::shared_ptr<App> app) {
    std::unique_lock<std::shared_mutex> lck(mutex_);
    domain_apps_[domain].insert(std::pair(app_name, app));
}

std::set<std::string> AppManager::get_domains() {
    std::set<std::string> domains;
    std::shared_lock<std::shared_mutex> lck(mutex_);
    for (auto & p : domain_apps_) {
        domains.insert(p.first);
    }
    return domains;
}

void AppManager::remove_domain(const std::string & domain) {
    std::unique_lock<std::shared_mutex> lck(mutex_);
    domain_apps_.erase(domain);
}

std::set<std::string> AppManager::get_domain_apps_name(const std::string & domain) {
    std::shared_lock<std::shared_mutex> lck(mutex_);
    auto it_domain = domain_apps_.find(domain);
    if (it_domain == domain_apps_.end()) {
        return {};
    }

    std::set<std::string> app_names;
    for (auto & p : it_domain->second) {
        app_names.insert(p.first);
    }
    return app_names;
}

void AppManager::remove_app(const std::string & domain, const std::string & app_name) {
    std::unique_lock<std::shared_mutex> lck(mutex_);
    auto it_domain = domain_apps_.find(domain);
    if (it_domain == domain_apps_.end()) {
        return;
    }
    it_domain->second.erase(app_name);
}