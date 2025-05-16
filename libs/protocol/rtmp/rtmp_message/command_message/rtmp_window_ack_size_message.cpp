#include<netinet/in.h>
#include "rtmp_window_ack_size_message.hpp"
using namespace mms;

int32_t RtmpWindowAckSizeMessage::decode(std::shared_ptr<RtmpMessage> rtmp_msg) {
    auto using_data = rtmp_msg->get_using_data();
    uint8_t * payload = (uint8_t*)using_data.data();
    int32_t len = using_data.size();
    if (len < 4) {
        return -1;
    }

    ack_window_size_ = ntohl(*(uint32_t*)payload);
    return 4;
}

std::shared_ptr<RtmpMessage> RtmpWindowAckSizeMessage::encode() const {
    std::shared_ptr<RtmpMessage> rtmp_msg = std::make_shared<RtmpMessage>(sizeof(ack_window_size_));
    rtmp_msg->chunk_stream_id_ = RTMP_CHUNK_ID_PROTOCOL_CONTROL_MESSAGE;
    rtmp_msg->timestamp_ = 0;
    rtmp_msg->message_type_id_ = RTMP_CHUNK_MESSAGE_TYPE_WINDOW_ACK_SIZE;
    rtmp_msg->message_stream_id_ = RTMP_MESSAGE_ID_PROTOCOL_CONTROL;
    auto unuse_data = rtmp_msg->get_unuse_data();
    *(uint32_t*)(unuse_data.data()) = htonl(ack_window_size_);
    rtmp_msg->inc_used_bytes(sizeof(ack_window_size_));
    return rtmp_msg;
}