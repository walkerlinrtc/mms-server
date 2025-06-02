#pragma once
#include <memory>
#include <mutex>
#include <unordered_map>
#include <shared_mutex>
#include <string>
#include <set>
#include <unordered_set>

namespace mms {
class App;
class AppManager {
public:
    AppManager();
    virtual ~AppManager();
    static AppManager & get_instance() {
        return instance_;
    }

    std::shared_ptr<App> get_app(const std::string & domain_name, const std::string & app_name);
    void add_app(const std::string & domain_name, const std::string & app_name, std::shared_ptr<App> app);

    void remove_domain(const std::string & domain_name);
    std::unordered_set<std::string> get_domains();

    std::unordered_set<std::string> get_domain_apps_name(const std::string & domain_name);
    void remove_app(const std::string & domain_name, const std::string & app_name);
private:
    std::shared_mutex mutex_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<App>>> domain_apps_;
    static AppManager instance_;
};
};