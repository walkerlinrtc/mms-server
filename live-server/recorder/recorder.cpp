#include "recorder.h"

using namespace mms;

Recorder::Recorder(const std::string & type,
                   ThreadWorker *worker, 
                   std::shared_ptr<PublishApp> app, 
                   std::weak_ptr<MediaSource> source, 
                   const std::string & domain_name, 
                   const std::string & app_name, 
                   const std::string & stream_name) : 
                   type_(type), worker_(worker), app_(app), source_(source), domain_name_(domain_name), 
                   app_name_(app_name), stream_name_(stream_name) {
    session_name_ = domain_name + "/" + app_name + "/" + stream_name;
}

Recorder::~Recorder() {

}

std::shared_ptr<Json::Value> Recorder::to_json() {
    return nullptr;
}

boost::asio::awaitable<std::shared_ptr<Json::Value>> Recorder::sync_to_json() {
    auto r = co_await sync_exec<Json::Value>([this]() {
        return to_json();
    });
    co_return r;
}

