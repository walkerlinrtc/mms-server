#pragma once
#include <memory>
#include <string>
#include <mutex>
#include <deque>
#include <shared_mutex>

#include "core/media_source.hpp"
namespace mms
{
    class TsSegment;
    class PublishApp;
    class MediaBridge;
    class ThreadWorker;
    class StreamSession;

    class HlsLiveMediaSource : public MediaSource
    {
    public:
        HlsLiveMediaSource(ThreadWorker *worker, std::weak_ptr<StreamSession> session, std::shared_ptr<PublishApp> app);
        virtual ~HlsLiveMediaSource();
        // 初始化相关函数
        bool init();

        virtual bool has_no_sinks_for_time(uint32_t milli_secs);
        void update_last_access_time();

        // 数据处理相关函数
        bool on_ts_segment(std::shared_ptr<TsSegment> TsSegment);
        // 管理及数据获取相关函数
        bool is_ready()
        {
            return is_ready_;
        }
        std::string get_m3u8();
        void set_m3u8(const std::string &v);
        std::shared_ptr<TsSegment> get_ts_segment(const std::string &ts_name);
        std::shared_ptr<MediaBridge> get_or_create_bridge(const std::string &id, std::shared_ptr<PublishApp> app, 
                                                          const std::string &stream_name);
    protected:
        std::shared_mutex ts_segments_mtx_;
        std::deque<std::shared_ptr<TsSegment>> ts_segments_;
        uint64_t curr_seq_no_ = 0;
        std::shared_mutex m3u8_mtx_;
        std::string m3u8_;
        bool is_ready_ = false;

    private:
        void update_m3u8();
    };
};