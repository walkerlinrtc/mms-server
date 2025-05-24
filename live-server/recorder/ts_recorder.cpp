#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/algorithm/string.hpp>

#include <filesystem>
#include <fstream>

#include "ts_recorder.h"
#include "core/ts_media_sink.hpp"
#include "core/media_source.hpp"
#include "log/log.h"

#include "config/config.h"
#include "protocol/ts/ts_segment.hpp"
#include "json/json.h"

#include "base/thread/thread_pool.hpp"

using namespace mms;

TsRecordSeg::TsRecordSeg() {

}

bool TsRecordSeg::load(const Json::Value & v) {
    create_at_ = std::stoll(v["create_at"].asString());
    update_at_ = std::stoll(v["update_at"].asString());
    duration_ = std::stoll(v["dur"].asString());
    file_name_ = v["file"].asString();
    seq_no_ = std::stoll(v["seq"].asString());
    start_pts_ = std::stoll(v["start_pts"].asString());
    server_ = v["server"].asString();
    return true;
}

TsRecorder::TsRecorder(ThreadWorker *worker, std::shared_ptr<PublishApp> app, 
                      std::weak_ptr<MediaSource> source, const std::string & domain_name, 
                      const std::string & app_name, const std::string & stream_name) : Recorder(worker, app, source, domain_name, app_name, stream_name) {
    sink_ = std::make_shared<TsMediaSink>(worker);
    ts_media_sink_ = std::static_pointer_cast<TsMediaSink>(sink_);
}

TsRecorder::~TsRecorder() {
    CORE_DEBUG("destroy TsRecorder");
}

bool TsRecorder::init() {
    auto s = source_.lock();
    if (!s) {
        return false;
    }

    auto self = shared_from_this();
    ts_media_sink_->on_close([this, self]() {
        close();
    });

    record_start_time_ = time(NULL);
    ts_media_sink_->on_ts_segment([this, self](std::shared_ptr<TsSegment> ts_seg)->boost::asio::awaitable<bool> {
        boost::system::error_code ec;
        std::string file_name = std::to_string(ts_seg->get_create_at()) + "_" + std::to_string(ts_seg->get_seqno()) + 
                                "_" + std::to_string(ts_seg->get_duration()) + "_" + std::to_string(ts_seg->get_start_pts()) + ".ts";
        std::string file_dir = Config::get_instance()->get_record_root_path() + "/" + domain_name_ + "/" + app_name_ + "/" + stream_name_ + "/ts/" + std::to_string(record_start_time_);
        try {
            // 写入文件
            std::filesystem::create_directories(file_dir);

            std::ofstream ts_file(file_dir + "/" + file_name, std::ios::out|std::ios::binary|std::ios::trunc);
            if (!ts_file.is_open()) {
                HLS_ERROR("create file:{} failed", file_dir + "/" + file_name);
                co_return true;
            }

            record_seg_count_++;
            auto ts_datas = ts_seg->get_ts_data();
            for (auto & ts_data : ts_datas) {
                ts_file.write(ts_data.data(), ts_data.size());
            }
            ts_file.close();
            
            
            TsRecordSeg seg;
            seg.create_at_ = ts_seg->get_create_at();
            seg.seq_no_ = seq_no_++;
            seg.duration_ = ts_seg->get_duration();
            seg.file_name_ = file_name;
            seg.start_pts_ = ts_seg->get_start_pts();
            seg.update_at_ = time(NULL);

            // Json::Value ts_rec;
            // ts_rec["seq"] = seq_no_++;
            // ts_rec["create_at"] = ts_seg->get_create_at();
            // ts_rec["dur"] = ts_seg->get_duration();
            // ts_rec["start_pts"] = ts_seg->get_start_pts();
            // ts_rec["file"] = file_name;
            // ts_rec["update_at"] = time(NULL);
            // auto & server_ip = Host::get_instance().get_wan_ip();
            // ts_rec["server"] = server_ip;
        } catch (const std::exception & e) {
            HLS_ERROR("create ts:{} failed, {}", file_dir + "/" + file_name, e.what());
            co_return true;
        }
            
        auto now = time(NULL);
        record_duration_ = now - record_start_time_;
        co_return true;
    });
    
    bool ret = s->add_media_sink(sink_);
    if (!ret) {
        return false;
    }

    return true;
}

void TsRecorder::close() {
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        auto s = source_.lock();
        if (s) {
            s->remove_media_sink(ts_media_sink_);
        }
        ts_media_sink_->on_close({});
        ts_media_sink_->on_ts_segment({});
        ts_media_sink_->close();
        co_return;
    }, boost::asio::detached);
}
