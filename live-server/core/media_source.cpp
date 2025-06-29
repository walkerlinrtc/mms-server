#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include "base/thread/thread_worker.hpp"
#include "spdlog/spdlog.h"

#include "media_source.hpp"
#include "media_sink.hpp"
#include "media_event.hpp"

#include "codec/codec.hpp"
#include "config/config.h"

#include "bridge/media_bridge.hpp"
#include "bridge/bridge_factory.hpp"
#include "recorder/recorder_factory.hpp"
#include "recorder/recorder.h"

#include "core/source_manager.hpp"
#include "app/publish_app.h"
#include "core/stream_session.hpp"

#include "base/network/bitrate_monitor.h"

using namespace mms;

MediaSource::MediaSource(const std::string & media_type, std::weak_ptr<StreamSession> session, std::shared_ptr<PublishApp> app, ThreadWorker *worker) : 
                                                                                                    media_type_(media_type), 
                                                                                                    app_(app), 
                                                                                                    worker_(worker)
{
    session_.store(session);
}

MediaSource::~MediaSource() {

}

Json::Value MediaSource::to_json() {
    Json::Value v;
    v["type"] = media_type_;
    v["domain"] = domain_name_;
    v["app"] = app_name_;
    v["stream"] = stream_name_;
    v["sinks"] = sinks_count_.load();
    v["create_at"] = create_at_;
    v["stream_time"] = time(NULL) - create_at_;
    v["client_ip"] = client_ip_;
    auto vcodec = video_codec_;
    if (vcodec) {
        v["vcodec"] = vcodec->to_json();
    }
    auto acodec = audio_codec_;
    if (acodec) {
        v["acodec"] = acodec->to_json();
    }
    return v;
}

boost::asio::awaitable<Json::Value> MediaSource::sync_to_json() {
    auto r = co_await sync_exec<Json::Value>([this]() {
        return to_json();
    });
    co_return r;
}

SourceStatus MediaSource::get_status() const {
    return status_.load();
}

void MediaSource::set_status(SourceStatus status) {
    status_.store(status);
    notify_status(status);
}

void MediaSource::notify_status(SourceStatus status) {
    std::shared_lock<std::shared_mutex> lck(sinks_mtx_);
    for (auto s : sinks_) {
        s->on_source_status_changed(status);
    }
}

std::shared_ptr<StreamSession> MediaSource::get_session() {
    auto s = session_.load().lock();
    return s;
}

bool MediaSource::set_session(std::shared_ptr<StreamSession> s) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    auto curr = session_.load().lock();
    if (curr != nullptr && curr != s) {
        return false;
    }

    session_.store(std::weak_ptr<StreamSession>(s));
    if (s) {
        worker_ = s->get_worker();
    }
    return true;
}

void MediaSource::set_source_info(const std::string & domain, const std::string & app_name, const std::string & stream_name) {
    domain_name_ = domain;
    app_name_ = app_name;
    stream_name_ = stream_name;
}

bool MediaSource::add_media_sink(std::shared_ptr<MediaSink> media_sink) {
    std::unique_lock<std::shared_mutex> lck(sinks_mtx_);
    sinks_.insert(media_sink);
    media_sink->set_source(this->shared_from_this());
    if (get_status() != E_SOURCE_STATUS_INIT) {
        media_sink->on_source_status_changed(status_.load());
    }
    sinks_count_++;
    return true;
}

bool MediaSource::remove_media_sink(std::shared_ptr<MediaSink> media_sink) {
    std::unique_lock<std::shared_mutex> lck(sinks_mtx_);
    for (auto it = sinks_.begin(); it != sinks_.end(); it++) {
        if (*it == media_sink) {
            sinks_count_--;
            sinks_.erase(it);
            media_sink->set_source(nullptr);
            CORE_DEBUG("remove sink from:{}/{}/{} type:{}", domain_name_, app_name_, stream_name_, media_type_);
            last_sinks_or_bridges_leave_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            break;
        }
    }
    return true;
}

bool MediaSource::remove_bridge(std::shared_ptr<MediaBridge> bridge) {
    std::lock_guard<std::shared_mutex> lck(bridges_mtx_);
    for (auto it = bridges_.begin(); it != bridges_.end(); it++) {
        if (it->second == bridge) {
            CORE_DEBUG("removing {} bridge of {}/{}/{}", bridge->type(), get_domain_name(), get_app_name(), get_stream_name());
            auto sink = it->second->get_media_sink();
            bridges_.erase(it);
            remove_media_sink(sink);
            last_sinks_or_bridges_leave_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            break;
        }
    }
    return true;
}

bool MediaSource::remove_bridge(const std::string & id) {
    std::lock_guard<std::shared_mutex> lck(bridges_mtx_);
    auto it = bridges_.find(id);
    if (it == bridges_.end()) {
        return false;
    }
    auto sink = it->second->get_media_sink();
    bridges_.erase(id);
    remove_media_sink(sink);
    last_sinks_or_bridges_leave_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return true;
}

bool MediaSource::has_no_sinks_for_time(uint32_t milli_secs) {
    std::shared_lock<std::shared_mutex> lck2(bridges_mtx_);
    if (sinks_count_ > 0 || bridges_.size() > 0) {
        return false;
    }

    int64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now_ms - last_sinks_or_bridges_leave_time_ < milli_secs) {
        return false;
    }
    return true;
}

std::shared_ptr<Recorder> MediaSource::get_or_create_recorder(const std::string & record_type, std::shared_ptr<PublishApp> app) {
    std::shared_ptr<Recorder> recorder;
    {
        std::unique_lock<std::shared_mutex> lck(recorder_mtx_);
        auto it = recorders_.find(record_type);
        if (it != recorders_.end()) {
            return it->second;
        }
    }
    
    CORE_INFO("create recorder {}/{}/{}/{}", app->get_domain_name(), app->get_app_name(), stream_name_, record_type);
    std::shared_ptr<MediaSource> source;
    auto media_type = get_media_type();
    if (record_type != get_media_type()) {
        auto bridge = get_or_create_bridge(media_type + "-" + record_type, app_, stream_name_);
        if (!bridge) {
            return nullptr;
        }

        source = bridge->get_media_source();
    } else {
        source = shared_from_this();
    }

    if (!source) {
        return nullptr;
    }
    
    recorder = RecorderFactory::create_recorder(worker_, record_type, app, std::weak_ptr<MediaSource>(source), app->get_domain_name(), app->get_app_name(), stream_name_);
    if (!recorder) {
        return nullptr;
    }
    
    if (!recorder->init()) {
        return nullptr;
    }
    
    source->add_recorder(record_type, recorder);
    return recorder;
}

void MediaSource::remove_recorder(std::shared_ptr<Recorder> recorder) {
    std::unique_lock<std::shared_mutex> lck(recorder_mtx_);
    for (auto it = recorders_.begin(); it != recorders_.end(); it++) {
        if (it->second == recorder) {
            recorders_.erase(it);
            break;
        }
    }
}

void MediaSource::add_recorder(const std::string & type, std::shared_ptr<Recorder> recorder) {
    std::unique_lock<std::shared_mutex> lck(recorder_mtx_);
    recorders_[type] = recorder;
}

bool MediaSource::is_stream_ready() {
    return stream_ready_;
}

void MediaSource::close() {
    if (closed_.test_and_set()) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [self, this]()->boost::asio::awaitable<void> {
        auto session = get_session();
        if (session) {
            session->stop(); 
        }
        CORE_DEBUG("close source:{}/{}/{}, type:{}", domain_name_, app_name_, stream_name_, get_media_type());
        {// 关闭所有的播放
            std::unique_lock<std::shared_mutex> lck(sinks_mtx_);
            for (auto sink : sinks_) {
                spdlog::debug("Closing sink ptr: {}, use_count: {}, worker: {}",
                                (void*)sink.get(),
                                sink.use_count(),
                                sink->get_worker() ? "ok" : "nullptr"
                            );
                boost::asio::co_spawn(sink->get_worker()->get_io_context(), [this, self, sink]()->boost::asio::awaitable<void> {
                    sink->close();
                    co_return;
                }, boost::asio::detached);
            }
            sinks_.clear();
        }
        // 因为bridge和recorder都是挂在sink后面的，所以sink关闭了，bridge和recorder也会被删除，不需要二次处理
        // 当然也可以在这里也关闭一下所以的bridge和recorder
        co_return;
    }, boost::asio::detached);
}
