#include <stdint.h>
#include <netinet/in.h>
#include "stream_begin_message.hpp"
using namespace mms;

RtmpStreamBeginMessage::RtmpStreamBeginMessage(uint32_t stream_id) : RtmpUserCtrlMessage(RTMP_USER_EVENT_STREAM_BEGIN) {
    stream_id_ = stream_id;
}

int32_t RtmpStreamBeginMessage::decode(std::shared_ptr<RtmpMessage> rtmp_msg) {
    ((void)rtmp_msg);
    return 0;
}

std::shared_ptr<RtmpMessage> RtmpStreamBeginMessage::encode() const {
    size_t s = 2 + 4;// type + id
    std::shared_ptr<RtmpMessage> rtmp_msg = std::make_shared<RtmpMessage>(s);
    rtmp_msg->chunk_stream_id_ = RTMP_CHUNK_ID_PROTOCOL_CONTROL_MESSAGE;
    rtmp_msg->timestamp_ = 0;
    rtmp_msg->message_type_id_ = RTMP_MESSAGE_TYPE_USER_CONTROL;
    rtmp_msg->message_stream_id_ = RTMP_MESSAGE_ID_PROTOCOL_CONTROL;

    auto unuse_data = rtmp_msg->get_unuse_data();
    uint8_t * payload = (uint8_t*)unuse_data.data();
    *(uint16_t *)payload = htons(event_type_);
    *(uint32_t *)(payload + 2) = htonl(stream_id_);
    rtmp_msg->inc_used_bytes(s);
    return rtmp_msg;
}