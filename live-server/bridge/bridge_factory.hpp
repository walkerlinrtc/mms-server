#pragma once
#include <memory>
#include <string>

namespace mms {
class MediaBridge;
class Session;
class PublishApp;
class ThreadWorker;
class MediaSource;
class BridgeFactory {
public:
    static std::shared_ptr<MediaBridge> create_bridge(ThreadWorker *worker, 
                                                     const std::string & id, 
                                                     std::shared_ptr<PublishApp> app,
                                                     std::weak_ptr<MediaSource> origin_source,
                                                     const std::string & domain_name, 
                                                     const std::string & app_name, 
                                                     const std::string & stream_name); 
};
};