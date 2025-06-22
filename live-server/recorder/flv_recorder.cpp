#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/algorithm/string.hpp>

#include <filesystem>
#include <fstream>

#include "flv_recorder.h"
#include "core/flv_media_sink.hpp"
#include "core/media_source.hpp"
#include "log/log.h"

#include "config/config.h"
#include "protocol/rtmp/flv/flv_tag.hpp"
#include "json/json.h"

#include "base/thread/thread_pool.hpp"
#include "recorder_manager.h"

using namespace mms;

FlvRecorder::FlvRecorder(ThreadWorker *worker, std::shared_ptr<PublishApp> app, 
                      std::weak_ptr<MediaSource> source, const std::string & domain_name, 
                      const std::string & app_name, const std::string & stream_name) : Recorder("flv",worker, app, source, domain_name, app_name, stream_name) {
    sink_ = std::make_shared<FlvMediaSink>(worker);
    flv_media_sink_ = std::static_pointer_cast<FlvMediaSink>(sink_);
}

FlvRecorder::~FlvRecorder() {
    CORE_DEBUG("destroy FlvRecorder:{}/{}/{}", get_domain_name(), get_app_name(), get_stream_name());
}

ssize_t writev_all(int fd, struct iovec *iov, int iovcnt) {
    ssize_t total_written = 0;
    while (iovcnt > 0) {
        ssize_t written = writev(fd, iov, iovcnt);
        if (written == -1) {
            if (errno == EINTR) {
                continue;  // 被信号中断，重试
            }
            return -1;     // 其他错误
        }

        total_written += written;

        // 调整 iov 和 iovcnt，跳过已写入的部分
        while (written > 0) {
            if (iov->iov_len <= (size_t)written) {
                // 当前 iov 已全部写入，跳到下一个
                written -= iov->iov_len;
                iov++;
                iovcnt--;
            } else {
                // 当前 iov 部分写入，调整指针和剩余长度
                iov->iov_base = (char *)iov->iov_base + written;
                iov->iov_len -= written;
                written = 0;
            }
        }
    }
    return total_written;
}

bool FlvRecorder::init() {
    auto s = source_.lock();
    if (!s) {
        return false;
    }

    auto self = shared_from_this();
    flv_media_sink_->on_close([this, self]() {
        close();
    });

    record_start_time_ = time(NULL);
    file_name_ = get_stream_name() + "_" + std::to_string(time(NULL)) + ".flv";
    file_dir_ = Config::get_instance()->get_record_root_path() + "/" + domain_name_ + "/" + app_name_ + "/" + stream_name_ + "/flv";
    std::filesystem::create_directories(file_dir_);
    flv_file_ = open((file_dir_ + "/" + file_name_).c_str(), O_WRONLY | O_CREAT, 0644);
    if (flv_file_ == -1) {
        return false;
    }

    flv_media_sink_->on_flv_tag([this, self](std::vector<std::shared_ptr<FlvTag>> & flv_tags)->boost::asio::awaitable<bool> {
        boost::system::error_code ec;
        auto now = time(NULL);
        int iov_count = 0;
        if (!has_write_flv_header_) {
            // 再发flv头部
            auto source = flv_media_sink_->get_source();//SourceManager::get_instance().get_source(get_session_name());
            uint8_t flv_header[9];
            FlvHeader header;
            header.flag.flags.audio = source->has_audio()?1:0;
            header.flag.flags.video = source->has_video()?1:0;
            header.encode(flv_header, 9);
            write_bufs_[iov_count].iov_base = flv_header;
            write_bufs_[iov_count].iov_len = 9;
            iov_count++;
            has_write_flv_header_ = true;
        } 

        int i = 0;
        int64_t bytes = 0;
        for (auto & flv_tag : flv_tags) {
            prev_tag_sizes_[i] = htonl(prev_tag_size_);
            write_bufs_[iov_count].iov_base = (char*)&prev_tag_sizes_[i];
            write_bufs_[iov_count].iov_len = 4;
            iov_count++;
            bytes += 4;

            std::string_view tag_payload = flv_tag->get_using_data();
            write_bufs_[iov_count].iov_base = (void*)tag_payload.data();
            write_bufs_[iov_count].iov_len = tag_payload.size();
            prev_tag_size_ = tag_payload.size();
            iov_count++;
            bytes += tag_payload.size();
            i++;
        }

        if (iov_count <= 0) {
            co_return true;
        }
        
        auto ret = writev_all(flv_file_, write_bufs_, iov_count);
        if (ret == -1) {
            CORE_ERROR("write flv file failed");
            co_return false;
        }
        write_bytes_ += bytes;
        record_duration_ = now - record_start_time_;
        co_return true;
    });
    
    bool ret = s->add_media_sink(flv_media_sink_);
    if (!ret) {
        return false;
    }

    RecorderManager::get_instance().add_recorder(self);
    return true;
}

Json::Value FlvRecorder::to_json() {
    Json::Value v;
    v["type"] = type_;
    v["domain"] = domain_name_;
    v["app"] = app_name_;
    v["stream"] = stream_name_;
    v["create_at"] = create_at_;
    v["filename"] = file_name_;
    v["duration"] = record_duration_;
    v["file_dir"] = file_dir_;
    v["write_bytes"] = write_bytes_;
    return v;
}

void FlvRecorder::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(shared_from_this());
    if (flv_file_ != -1) {
        ::close(flv_file_);
    }

    RecorderManager::get_instance().remove_recorder(self);
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        auto s = source_.lock();
        flv_media_sink_->on_close({});
        flv_media_sink_->on_flv_tag({});
        flv_media_sink_->close();
        if (s) {
            s->remove_media_sink(flv_media_sink_);
        }
        co_return;
    }, boost::asio::detached);
}
