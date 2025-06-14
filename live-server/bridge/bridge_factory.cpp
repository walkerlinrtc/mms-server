/*
 * @Author: jbl19860422
 * @Date: 2023-12-03 11:50:29
 * @LastEditTime: 2023-12-03 11:50:39
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\server\transcode\bridge_factory.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "log/log.h"
#include "bridge_factory.hpp"
#include "base/thread/thread_worker.hpp"

#include "rtmp/rtmp_to_flv.hpp"
#include "rtmp/rtmp_to_ts.hpp"
#include "rtmp/rtmp_to_rtsp.hpp"
#include "rtmp/rtmp_to_m4s.hpp"

#include "rtsp/rtsp_to_flv.hpp"
#include "rtsp/rtsp_to_rtmp.hpp"
#include "rtsp/rtsp_to_ts.hpp"

#include "flv/flv_to_rtmp.hpp"
#include "flv/flv_to_ts.hpp"
#include "flv/flv_to_rtsp.hpp"

#include "webrtc/webrtc_to_flv.hpp"
#include "webrtc/webrtc_to_rtmp.hpp"
#include "webrtc/webrtc_to_ts.hpp"

#include "ts/ts_to_hls.hpp"
#include "mp4/m4s_to_mpd.hpp"

#include "app/publish_app.h"
#include "config/app_config.h"

using namespace mms;
std::shared_ptr<MediaBridge> BridgeFactory::create_bridge(ThreadWorker *worker, 
                                                         const std::string & id, 
                                                         std::shared_ptr<PublishApp> app,
                                                         std::weak_ptr<MediaSource> origin_source,
                                                         const std::string & domain_name, 
                                                         const std::string & app_name, 
                                                         const std::string & stream_name) {
    CORE_INFO("create bridge:{}", id);                                    
    if (id == "rtmp-flv" && app->get_conf()->bridge_config().rtmp_to_flv()) {
        return std::make_shared<RtmpToFlv>(worker, app, origin_source, domain_name, app_name, stream_name);
    } else if (id == "rtmp-rtsp{rtp[es]}" && app->get_conf()->bridge_config().rtmp_to_rtsp()) {
        return std::make_shared<RtmpToRtsp>(worker, app, origin_source, domain_name, app_name, stream_name);
    } else if (id == "rtmp-ts" && app->get_conf()->bridge_config().rtmp_to_hls()) {
        return std::make_shared<RtmpToTs>(worker, app, origin_source, domain_name, app_name, stream_name);
    } else if (id == "rtmp-m4s") {
        return std::make_shared<RtmpToM4s>(worker, app, origin_source, domain_name, app_name, stream_name);
    } else if (id == "m4s-mpd") {
        return std::make_shared<M4sToMpd>(worker, app, origin_source, domain_name, app_name, stream_name);
    } else if (id == "ts-hls" && app->get_conf()->bridge_config().rtmp_to_hls()) {
        return std::make_shared<TsToHls>(worker, app, origin_source, domain_name, app_name, stream_name);
    } else if (id == "rtsp{rtp[es]}-flv" && app->get_conf()->bridge_config().rtsp_to_flv()) {
        return std::make_shared<RtspToFlv>(worker, app, origin_source, domain_name, app_name, stream_name);
    } else if (id == "rtsp{rtp[es]}-rtmp" && app->get_conf()->bridge_config().rtsp_to_rtmp()) {
        return std::make_shared<RtspToRtmp>(worker, app, origin_source, domain_name, app_name, stream_name);
    } else if (id == "rtsp{rtp[es]}-ts" && app->get_conf()->bridge_config().rtsp_to_hls()) {
        return std::make_shared<RtspToTs>(worker, app, origin_source, domain_name, app_name, stream_name);
    } else if (id == "flv-rtmp" && app->get_conf()->bridge_config().flv_to_rtmp()) {
        return std::make_shared<FlvToRtmp>(worker, app, origin_source, domain_name, app_name, stream_name);
    } else if (id == "flv-ts" && app->get_conf()->bridge_config().flv_to_hls()) {
        return std::make_shared<FlvToTs>(worker, app, origin_source, domain_name, app_name, stream_name);
    } else if (id == "flv-rtsp{rtp[es]}" && app->get_conf()->bridge_config().flv_to_rtsp()) {
        return std::make_shared<FlvToRtsp>(worker, app, origin_source, domain_name, app_name, stream_name);
    } else if (id == "webrtc{rtp[es]}-flv" && app->get_conf()->bridge_config().webrtc_to_flv()) {
        return std::make_shared<WebRtcToFlv>(worker, app, origin_source, domain_name, app_name, stream_name);
    } else if (id == "webrtc{rtp[es]}-rtmp" && app->get_conf()->bridge_config().webrtc_to_rtmp()) {
        return std::make_shared<WebRtcToRtmp>(worker, app, origin_source, domain_name, app_name, stream_name);
    } else if (id == "webrtc{rtp[es]}-ts" && app->get_conf()->bridge_config().webrtc_to_hls()) {
        return std::make_shared<WebRtcToTs>(worker, app, origin_source, domain_name, app_name, stream_name);
    } 
    return nullptr;
}