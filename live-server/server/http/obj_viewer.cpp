#include "obj_viewer.h"
using namespace mms;
#include "core/rtmp_media_source.hpp"
#include "core/rtmp_media_sink.hpp"
#include "core/flv_media_source.hpp"
#include "core/flv_media_sink.hpp"

#include "server/http/http_server_session.hpp"
#include "server/rtmp/rtmp_server_session.hpp"
#include "server/http/http_flv_server_session.hpp"

#include "bridge/rtmp/rtmp_to_flv.hpp"
#include "bridge/rtmp/rtmp_to_ts.hpp"
#include "bridge/rtmp/rtmp_to_m4s.hpp"
#include "bridge/rtmp/rtmp_to_rtsp.hpp"

#include "bridge/flv/flv_to_rtmp.hpp"
#include "bridge/flv/flv_to_rtsp.hpp"
#include "bridge/flv/flv_to_ts.hpp"

#include "recorder/flv_recorder.h"
#include "recorder/ts_recorder.h"
#include "recorder/m4s_recorder.h"

Json::Value ObjViewer::to_json() {
    Json::Value v;
    // base
    v["Session"] = ObjTracker<Session>::get_use_count();
    v["StreamSession"] = ObjTracker<StreamSession>::get_use_count();
    v["HttpServerSession"] = ObjTracker<HttpServerSession>::get_use_count();
    v["HttpFlvServerSession"] = ObjTracker<HttpFlvServerSession>::get_use_count();
    v["RtmpServerSession"] = ObjTracker<RtmpServerSession>::get_use_count();
    // rtmp
    v["RtmpMediaSource"] = ObjTracker<RtmpMediaSource>::get_use_count();
    v["RtmpMediaSink"] = ObjTracker<RtmpMediaSink>::get_use_count();
    // rtmp-bridge
    v["RtmpToFlv"] = ObjTracker<RtmpToFlv>::get_use_count();
    v["RtmpToTs"] = ObjTracker<RtmpToTs>::get_use_count();
    v["RtmpToM4s"] = ObjTracker<RtmpToM4s>::get_use_count();
    v["RtmpToRtsp"] = ObjTracker<RtmpToRtsp>::get_use_count();

    // flv
    v["FlvMediaSource"] = ObjTracker<FlvMediaSource>::get_use_count();
    v["FlvMediaSink"] = ObjTracker<FlvMediaSink>::get_use_count();
    // flv-bridge
    v["FlvToRtmp"] = ObjTracker<FlvToRtmp>::get_use_count();
    v["FlvToTs"] = ObjTracker<FlvToTs>::get_use_count();
    v["FlvToRtsp"] = ObjTracker<FlvToRtsp>::get_use_count();


    // recorder
    v["FlvRecorder"] = ObjTracker<FlvRecorder>::get_use_count();
    v["TsRecorder"] = ObjTracker<TsRecorder>::get_use_count();
    v["M4sRecorder"] = ObjTracker<M4sRecorder>::get_use_count();
    return v;
}