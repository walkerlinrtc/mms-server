#include "recorder_factory.hpp"
#include "base/thread/thread_worker.hpp"
#include "log/log.h"

#include "ts_recorder.h"
#include "flv_recorder.h"

using namespace mms;
std::shared_ptr<Recorder> RecorderFactory::create_recorder(ThreadWorker *worker, 
                                                           const std::string & id, 
                                                           std::shared_ptr<PublishApp> app,
                                                           std::weak_ptr<MediaSource> origin_source,
                                                           const std::string & domain_name, 
                                                           const std::string & app_name, 
                                                           const std::string & stream_name) {
    CORE_INFO("try to create recorder for {}/{}/{}, id:{}", domain_name, app_name, stream_name, id);
    if (id == "ts") {
        return std::make_shared<TsRecorder>(worker, app, origin_source, domain_name, app_name, stream_name);
    } else if (id == "flv") {
        return std::make_shared<FlvRecorder>(worker, app, origin_source, domain_name, app_name, stream_name);
    }
    return nullptr;
}