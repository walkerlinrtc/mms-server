/*
 * @Author: jbl19860422
 * @Date: 2023-12-02 21:39:27
 * @LastEditTime: 2023-12-27 18:43:25
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\transcode\media_bridge.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
#include <memory>
#include <string>

namespace mms {
class MediaSource;
class MediaSink;
class ThreadWorker;
class PublishApp;

class MediaBridge : public std::enable_shared_from_this<MediaBridge> {
public:
    MediaBridge(ThreadWorker *worker, std::shared_ptr<PublishApp> app, std::weak_ptr<MediaSource> origin_source, const std::string & domain_name, const std::string & app_name, const std::string & stream_name);
    virtual ~MediaBridge();
    virtual bool init() = 0;
    ThreadWorker *get_worker() const {
        return worker_;
    }

    std::shared_ptr<PublishApp> get_app() const {
        return publish_app_;
    }

    std::shared_ptr<MediaSource> get_media_source() {
        return source_;
    }

    std::shared_ptr<MediaSink> get_media_sink() {
        return sink_;
    }

    inline std::string & get_session_name() {
        return session_name_;
    }

    const std::string & type() const {
        return type_;
    }
    virtual void close() = 0;
protected:
    std::string type_;
    std::atomic_flag closed_ = ATOMIC_FLAG_INIT;
    ThreadWorker *worker_;
    std::shared_ptr<PublishApp> publish_app_;
    std::weak_ptr<MediaSource> origin_source_;
    std::shared_ptr<MediaSource> source_;
    std::shared_ptr<MediaSink> sink_;
    std::string domain_name_;
    std::string app_name_;
    std::string stream_name_;
    std::string session_name_;
};
};