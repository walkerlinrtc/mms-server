
#include "media_bridge.hpp"
#include "core/media_source.hpp"
#include "core/media_sink.hpp"

using namespace mms;

MediaBridge::MediaBridge(ThreadWorker *worker, 
                         std::shared_ptr<PublishApp> app, 
                         std::weak_ptr<MediaSource> origin_source, 
                         const std::string & domain_name, 
                         const std::string & app_name, 
                         const std::string & stream_name) : worker_(worker), publish_app_(app), origin_source_(origin_source), domain_name_(domain_name), app_name_(app_name), stream_name_(stream_name) {
    session_name_ = domain_name + "/" + app_name + "/" + stream_name;
}

MediaBridge::~MediaBridge() {

}