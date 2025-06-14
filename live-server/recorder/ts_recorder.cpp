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
    CORE_DEBUG("create TsRecorder");
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
        file_dir_ = Config::get_instance()->get_record_root_path() + "/" + domain_name_ + "/" + app_name_ + "/" + stream_name_ + "/ts/" + std::to_string(record_start_time_);
        std::string file_name;
        try {
            // 写入文件
            std::filesystem::create_directories(file_dir_);
            auto seq_no = seq_no_;
            file_name = std::to_string(ts_seg->get_create_at()) + "_" + std::to_string(seq_no) + 
                                "_" + std::to_string(ts_seg->get_duration()) + "_" + std::to_string(ts_seg->get_start_pts()) + ".ts";

            std::ofstream ts_file(file_dir_ + "/" + file_name, std::ios::out|std::ios::binary|std::ios::trunc);
            if (!ts_file.is_open()) {
                HLS_ERROR("create file:{} failed", file_dir_ + "/" + file_name);
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
            ts_segs_.emplace_back(seg);
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
            HLS_ERROR("create ts:{} failed, {}", file_dir_ + "/" + file_name, e.what());
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

    CORE_DEBUG("close TsRecorder");
    auto self(shared_from_this());
    boost::asio::co_spawn(worker_->get_io_context(), [this, self]()->boost::asio::awaitable<void> {
        auto s = source_.lock();
        if (s) {
            s->remove_media_sink(ts_media_sink_);
        }
        ts_media_sink_->on_close({});
        ts_media_sink_->on_ts_segment({});
        ts_media_sink_->close();

        if (!file_dir_.empty()) {
            gen_m3u8();
        }
        co_return;
    }, boost::asio::detached);
}

void TsRecorder::gen_m3u8() {
    if (ts_segs_.size() <= 0) {
        return;
    }

    CORE_DEBUG("generate ts record m3u8");
    std::stringstream ss;
    ss << "#EXTM3U\r\n";
    ss << "#EXT-X-VERSION:3\r\n";
    // 获取最大切片时长
    int64_t max_duration = 0;
    for (auto & seg : ts_segs_) {
        max_duration = std::max(max_duration, seg.duration_);
    }

    ss << "#EXT-X-TARGETDURATION:" << (int)ceil(max_duration / 1000.0) << "\r\n";
    ss << "#EXT-X-MEDIA-SEQUENCE:" << ts_segs_[0].seq_no_ << "\r\n";

    ss.precision(3);
    ss.setf(std::ios::fixed, std::ios::floatfield);
    for (auto & seg : ts_segs_) {
        ss << "#EXTINF:" << seg.duration_ / 1000.0 << "\r\n";
        ss << seg.file_name_ << "\r\n";
    }
    ss << "#EXT-X-ENDLIST\r\n";
    auto m3u8 = ss.str();
    std::string file_name = get_stream_name() + "_" + std::to_string(record_start_time_) + ".m3u8";
    std::ofstream m3u8_file(file_dir_ + "/" + file_name, std::ios::out);
    m3u8_file << m3u8;
    m3u8_file.close();
}