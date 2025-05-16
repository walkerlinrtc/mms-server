#include "recorder.h"
using namespace mms;

Recorder::Recorder(ThreadWorker *worker, 
                   std::shared_ptr<PublishApp> app, 
                   std::weak_ptr<MediaSource> source, 
                   const std::string & domain_name, 
                   const std::string & app_name, 
                   const std::string & stream_name) : 
                   worker_(worker), app_(app), source_(source), domain_name_(domain_name), 
                   app_name_(app_name), stream_name_(stream_name) {
    session_name_ = domain_name + "/" + app_name + "/" + stream_name;
}

Recorder::~Recorder() {

}

