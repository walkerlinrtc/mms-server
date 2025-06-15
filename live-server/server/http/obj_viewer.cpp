#include "obj_viewer.h"
using namespace mms;
#include "core/rtmp_media_source.hpp"
#include "core/rtmp_media_sink.hpp"

#include "server/rtmp/rtmp_server_session.hpp"

#include "bridge/rtmp/rtmp_to_flv.hpp"

Json::Value ObjViewer::to_json() {
    Json::Value v;
    // rtmp
    v["RtmpServerSession"] = ObjTracker<RtmpServerSession>::get_use_count();
    v["RtmpMediaSource"] = ObjTracker<RtmpMediaSource>::get_use_count();
    v["RtmpMediaSink"] = ObjTracker<RtmpMediaSink>::get_use_count();
    v["RtmpToFlv"] = ObjTracker<RtmpToFlv>::get_use_count();
    return v;
}