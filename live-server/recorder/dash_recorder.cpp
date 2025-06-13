#include "dash_recorder.h"

#include <boost/algorithm/string.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <filesystem>
#include <fstream>

#include "base/thread/thread_pool.hpp"
#include "config/config.h"
#include "core/media_source.hpp"
#include "core/mp4_media_sink.hpp"
#include "json/json.h"
#include "log/log.h"
#include "protocol/mp4/mp4_segment.h"

using namespace mms;

M4sRecordSeg::M4sRecordSeg() {}

bool M4sRecordSeg::load(const Json::Value &v) {
    create_at_ = std::stoll(v["create_at"].asString());
    update_at_ = std::stoll(v["update_at"].asString());
    duration_ = std::stoll(v["dur"].asString());
    file_name_ = v["file"].asString();
    seq_no_ = std::stoll(v["seq"].asString());
    start_pts_ = std::stoll(v["start_pts"].asString());
    server_ = v["server"].asString();
    return true;
}

DashRecorder::DashRecorder(ThreadWorker *worker, std::shared_ptr<PublishApp> app,
                           std::weak_ptr<MediaSource> source, const std::string &domain_name,
                           const std::string &app_name, const std::string &stream_name)
    : Recorder(worker, app, source, domain_name, app_name, stream_name) {
    sink_ = std::make_shared<Mp4MediaSink>(worker);
    mp4_media_sink_ = std::static_pointer_cast<Mp4MediaSink>(sink_);
    CORE_DEBUG("create DashRecorder");
}

DashRecorder::~DashRecorder() { CORE_DEBUG("destroy DashRecorder"); }

bool DashRecorder::init() {
    auto s = source_.lock();
    if (!s) {
        return false;
    }

    auto self = shared_from_this();
    mp4_media_sink_->on_close([this, self]() { 
        CORE_DEBUG("DashRecorder::mp4_media_sink::on_close");
        close(); 
    });

    record_start_time_ = time(NULL);
    file_dir_ = Config::get_instance()->get_record_root_path() + "/" + domain_name_ + "/" + app_name_ + "/" +
                stream_name_ + "/dash/" + std::to_string(record_start_time_);
    mp4_media_sink_->set_audio_init_segment_cb(
        [this, self](std::shared_ptr<Mp4Segment> seg) -> boost::asio::awaitable<bool> {
            co_return on_audio_init_segment(seg);
        });

    mp4_media_sink_->set_video_init_segment_cb(
        [this, self](std::shared_ptr<Mp4Segment> seg) -> boost::asio::awaitable<bool> {
            co_return on_video_init_segment(seg);
        });

    mp4_media_sink_->set_audio_mp4_segment_cb(
        [this, self](std::shared_ptr<Mp4Segment> seg) -> boost::asio::awaitable<bool> {
            co_return on_audio_segment(seg);
        });

    mp4_media_sink_->set_video_mp4_segment_cb(
        [this, self](std::shared_ptr<Mp4Segment> seg) -> boost::asio::awaitable<bool> {
            co_return on_video_segment(seg);
        });

    bool ret = s->add_media_sink(mp4_media_sink_);
    if (!ret) {
        return false;
    }

    return true;
}

bool DashRecorder::on_audio_init_segment(std::shared_ptr<Mp4Segment> m4s_seg) {
    CORE_DEBUG("DashRecorder::on_audio_init_segment file_name:{}", m4s_seg->get_filename());
    audio_init_seg_ = m4s_seg;
    std::filesystem::create_directories(file_dir_);
    std::ofstream m4s_file(file_dir_ + "/" + m4s_seg->get_filename(),
                           std::ios::out | std::ios::binary | std::ios::trunc);
    if (!m4s_file.is_open()) {
        return false;
    }
    auto mp4_data = m4s_seg->get_used_buf();
    m4s_file.write(mp4_data.data(), mp4_data.size());
    m4s_file.close();
    // update_mpd();
    return true;
}

bool DashRecorder::on_video_init_segment(std::shared_ptr<Mp4Segment> m4s_seg) {
    video_init_seg_ = m4s_seg;
    std::filesystem::create_directories(file_dir_);
    std::ofstream m4s_file(file_dir_ + "/" + m4s_seg->get_filename(),
                           std::ios::out | std::ios::binary | std::ios::trunc);
    if (!m4s_file.is_open()) {
        return false;
    }
    auto mp4_data = m4s_seg->get_used_buf();
    m4s_file.write(mp4_data.data(), mp4_data.size());
    m4s_file.close();
    // update_mpd();
    return true;
}

bool DashRecorder::on_audio_segment(std::shared_ptr<Mp4Segment> m4s_seg) {
    std::filesystem::create_directories(file_dir_);
    std::ofstream m4s_file(file_dir_ + "/" + m4s_seg->get_filename(),
                           std::ios::out | std::ios::binary | std::ios::trunc);
    if (!m4s_file.is_open()) {
        return false;
    }
    auto mp4_data = m4s_seg->get_used_buf();
    m4s_file.write(mp4_data.data(), mp4_data.size());
    m4s_file.close();

    M4sRecordSeg seg;
    seg.create_at_ = m4s_seg->get_create_at();
    seg.seq_no_ = audio_seq_no_++;
    seg.duration_ = m4s_seg->get_duration();
    seg.file_name_ = m4s_seg->get_filename();
    seg.start_pts_ = m4s_seg->get_start_timestamp();
    seg.update_at_ = time(NULL);
    audio_m4s_segs_.emplace_back(seg);
    record_audio_seg_count_++;
    auto now = time(NULL);
    record_duration_ = now - record_start_time_;
    return true;
}

bool DashRecorder::on_video_segment(std::shared_ptr<Mp4Segment> m4s_seg) {
    std::filesystem::create_directories(file_dir_);
    std::ofstream m4s_file(file_dir_ + "/" + m4s_seg->get_filename(),
                           std::ios::out | std::ios::binary | std::ios::trunc);
    if (!m4s_file.is_open()) {
        return false;
    }
    auto mp4_data = m4s_seg->get_used_buf();
    m4s_file.write(mp4_data.data(), mp4_data.size());
    m4s_file.close();

    M4sRecordSeg seg;
    seg.create_at_ = m4s_seg->get_create_at();
    seg.seq_no_ = video_seq_no_++;
    seg.duration_ = m4s_seg->get_duration();
    seg.file_name_ = m4s_seg->get_filename();
    seg.start_pts_ = m4s_seg->get_start_timestamp();
    seg.update_at_ = time(NULL);
    video_m4s_segs_.emplace_back(seg);
    record_video_seg_count_++;
    auto now = time(NULL);
    record_duration_ = now - record_start_time_;
    return true;
}

void DashRecorder::close() {
    CORE_DEBUG("close DashRecorder");
    if (closed_.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto self(shared_from_this());
    boost::asio::co_spawn(
        worker_->get_io_context(),
        [this, self]() -> boost::asio::awaitable<void> {
            auto s = source_.lock();
            if (s) {
                s->remove_media_sink(mp4_media_sink_);
            }
            mp4_media_sink_->on_close({});
            mp4_media_sink_->set_audio_init_segment_cb({});
            mp4_media_sink_->set_video_init_segment_cb({});
            mp4_media_sink_->set_audio_mp4_segment_cb({});
            mp4_media_sink_->set_video_mp4_segment_cb({});
            mp4_media_sink_->close();

            if (!file_dir_.empty()) {
                gen_mpd();
            }
            co_return;
        },
        boost::asio::detached);
}

std::string format_duration_mpd(int64_t duration_ms) {
    int64_t total_seconds = duration_ms / 1000;
    int milliseconds = duration_ms % 1000;

    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;

    std::ostringstream ss;
    ss << "PT";
    if (hours > 0) {
        ss << hours << "H";
    }
    if (minutes > 0) {
        ss << minutes << "M";
    }
    if (milliseconds > 0) {
        // seconds + fractional part
        ss << std::fixed << std::setprecision(3) << seconds + milliseconds / 1000.0 << "S";
    } else if (seconds > 0 || (hours == 0 && minutes == 0)) {
        ss << seconds << "S";
    }

    return ss.str();
}

std::string format_max_segment_duration(int64_t duration_ms) {
    double seconds = duration_ms / 1000.0;

    std::ostringstream ss;
    ss << "PT" << std::fixed << std::setprecision(3) << seconds << "S";
    return ss.str();
}

void DashRecorder::gen_mpd() {
    if (audio_m4s_segs_.empty() && video_m4s_segs_.empty()) {
        return;
    }

    double last_duration = 0;
    if (!audio_m4s_segs_.empty()) {
        last_duration = audio_m4s_segs_.back().duration_;
    }
    if (!video_m4s_segs_.empty()) {
        last_duration = std::max((double)video_m4s_segs_.back().duration_, last_duration);
    }

    int64_t total_audio_duration = 0;
    int64_t total_video_duration = 0;
    int64_t max_seg_dur = 0;
    for (auto &as : audio_m4s_segs_) {
        total_audio_duration += as.duration_;
        max_seg_dur = std::max(as.duration_, max_seg_dur);
    }

    for (auto &vs : video_m4s_segs_) {
        total_video_duration += vs.duration_;
        max_seg_dur = std::max(vs.duration_, max_seg_dur);
    }

    int64_t total_duration = std::max(total_audio_duration, total_video_duration);
    std::string mediaPresentationDuration = format_duration_mpd(total_duration);
    std::string maxSegmentDuration = format_max_segment_duration(max_seg_dur);

    std::stringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>" << std::endl
       << "<MPD " << std::endl
       << "xmlns=\"urn:mpeg:dash:schema:mpd:2011\" " << std::endl
       << "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" " << std::endl
       << "xmlns:xlink=\"http://www.w3.org/1999/xlink\"" << std::endl
       << "profiles=\"urn:mpeg:dash:profile:isoff-live:2011\"" << std::endl
       << "xsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 "
          "http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd\" "
       << std::endl
       << "type=\"static\" " << std::endl
       << "mediaPresentationDuration=\"" << mediaPresentationDuration << "\"" << std::endl
       << "maxSegmentDuration=\"" << maxSegmentDuration << "\"" << std::endl
       << "minBufferTime=\"PT4.0S\"" << std::endl
       << ">" << std::endl;

    ss << "    <BaseURL>" << stream_name_ << "/dash/" << record_start_time_ << "/" << "</BaseURL>"
       << std::endl;
    ss << "    <Period start=\"PT0S\">" << std::endl;

    // -------- AUDIO ----------
    if (audio_init_seg_ && !audio_m4s_segs_.empty()) {
        ss << "        <AdaptationSet mimeType=\"audio/mp4\" segmentAlignment=\"true\" startWithSAP=\"1\">"
           << std::endl;
        ss << "            <Representation id=\"audio\" bandwidth=\"64000\" codecs=\"mp4a.40.2\">"
           << std::endl;
        ss << "                <SegmentTemplate initialization=\"audio-init.m4s\" "
           << "media=\"audio-$Number$.m4s\" "
           << "startNumber=\"" << audio_m4s_segs_[0].seq_no_ << "\" "
           << "timescale=\"1000\">" << std::endl;
        ss << "                    <SegmentTimeline>" << std::endl;
        for (size_t i = 0; i < audio_m4s_segs_.size(); ++i) {
            const auto &seg = audio_m4s_segs_[i];
            ss << "                        <S t=\"" << seg.start_pts_ << "\" d=\"" << seg.duration_ << "\" />"
               << std::endl;
        }
        ss << "                    </SegmentTimeline>" << std::endl;
        ss << "                </SegmentTemplate>" << std::endl;
        ss << "            </Representation>" << std::endl;
        ss << "        </AdaptationSet>" << std::endl;
    }

    // -------- VIDEO ----------
    if (video_init_seg_ && !video_m4s_segs_.empty()) {
        ss << "        <AdaptationSet mimeType=\"video/mp4\" segmentAlignment=\"true\" startWithSAP=\"1\">"
           << std::endl;
        ss << "            <Representation id=\"video\" bandwidth=\"1000000\" codecs=\"avc1.64001e\">"
           << std::endl;
        ss << "                <SegmentTemplate initialization=\"video-init.m4s\" "
           << "media=\"video-$Number$.m4s\" "
           << "startNumber=\"" << video_m4s_segs_[0].seq_no_ << "\" "
           << "timescale=\"1000\">" << std::endl;
        ss << "                    <SegmentTimeline>" << std::endl;
        for (size_t i = 0; i < video_m4s_segs_.size(); ++i) {
            const auto &seg = video_m4s_segs_[i];
            ss << "                        <S t=\"" << seg.start_pts_ << "\" d=\"" << seg.duration_ << "\" />"
               << std::endl;
        }
        ss << "                    </SegmentTimeline>" << std::endl;
        ss << "                </SegmentTemplate>" << std::endl;
        ss << "            </Representation>" << std::endl;
        ss << "        </AdaptationSet>" << std::endl;
    }

    ss << "    </Period>" << std::endl;
    ss << "</MPD>" << std::endl;

    std::string mpd_path = file_dir_ + "/manifest.mpd";
    std::ofstream ofs(mpd_path, std::ios::out | std::ios::trunc);
    if (ofs.is_open()) {
        ofs << ss.str();
        ofs.close();
        spdlog::info("Generated MPD: {}", mpd_path);
    } else {
        spdlog::error("Failed to write MPD to: {}", mpd_path);
    }
}