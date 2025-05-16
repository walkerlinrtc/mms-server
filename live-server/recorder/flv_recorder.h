#pragma once
#include <string>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>

#include "recorder.h"
#include "json/json.h"

namespace mms {
class ThreadWorker;
class FlvMediaSink;
class PublishApp;

class FlvRecorder : public Recorder {
public:
    FlvRecorder(ThreadWorker *worker, std::shared_ptr<PublishApp> app, 
               std::weak_ptr<MediaSource> source, const std::string & domain_name, const std::string & app_name, 
               const std::string & stream_name);
    virtual ~FlvRecorder();

    bool init() override;
    void close() override;
private:
    bool has_write_flv_header_ = false;
    uint32_t prev_tag_size_ = 0;
    uint32_t prev_tag_sizes_[100];
    struct iovec write_bufs_[100];
    std::shared_ptr<FlvMediaSink> flv_media_sink_;
    int64_t record_duration_ = 0;
    int64_t record_start_time_ = 0;
    int flv_file_ = -1;
};
};