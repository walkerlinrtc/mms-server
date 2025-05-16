#include "rtmp_access_sample_message.hpp"
using namespace mms;

RtmpAccessSampleMessage::RtmpAccessSampleMessage(bool video, bool audio) {
    command_name_.set_value("|RtmpSampleAccess");
    audio_.set_value(audio);
    video_.set_value(video);
}

int32_t RtmpAccessSampleMessage::decode(std::shared_ptr<RtmpMessage> rtmp_msg) {
    ((void)rtmp_msg);
    return 0;
}

std::shared_ptr<RtmpMessage> RtmpAccessSampleMessage::encode() const {
    size_t s = 0;
    s += command_name_.size();
    s += audio_.size();
    s += video_.size();

    std::shared_ptr<RtmpMessage> rtmp_msg = std::make_shared<RtmpMessage>(s);
    rtmp_msg->chunk_stream_id_ = RTMP_CHUNK_ID_PROTOCOL_CONTROL_MESSAGE;
    rtmp_msg->timestamp_ = 0;
    rtmp_msg->message_type_id_ = RTMP_MESSAGE_TYPE_AMF0_DATA;
    rtmp_msg->message_stream_id_ = RTMP_MESSAGE_ID_PROTOCOL_CONTROL;
    // window ack_size
    auto unuse_data = rtmp_msg->get_unuse_data();
    uint8_t * payload = (uint8_t*)unuse_data.data();

    int32_t len = s;
    int32_t consumed = command_name_.encode(payload, len);
    if (consumed < 0) {
        return nullptr;
    }
    payload += consumed;
    len -= consumed;

    consumed = video_.encode(payload, len);
    if (consumed < 0) {
        return nullptr;
    }
    payload += consumed;
    len -= consumed;

    consumed = audio_.encode(payload, len);
    if (consumed < 0) {
        return nullptr;
    }
    payload += consumed;
    len -= consumed;

    rtmp_msg->inc_used_bytes(s);
    return rtmp_msg;
}
