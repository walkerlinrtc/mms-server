#include <netinet/in.h>
#include "rtmp_acknowledge_message.hpp"
using namespace mms;

RtmpAcknwledgeMessage::RtmpAcknwledgeMessage(size_t s) : acknowledge_(s) {

}

RtmpAcknwledgeMessage::RtmpAcknwledgeMessage() {

}

int32_t RtmpAcknwledgeMessage::decode(std::shared_ptr<RtmpMessage> rtmp_msg) {
    auto using_data = rtmp_msg->get_using_data();
    uint8_t * payload = (uint8_t*)using_data.data();
    int32_t len = using_data.size();
    if (len < 4) {
        return -1;
    }
    acknowledge_ = ntohl(*(uint32_t*)payload);
    return 4;
}

std::shared_ptr<RtmpMessage> RtmpAcknwledgeMessage::encode() const {
    std::shared_ptr<RtmpMessage> rtmp_msg = std::make_shared<RtmpMessage>(4);
    rtmp_msg->chunk_stream_id_ = RTMP_CHUNK_ID_PROTOCOL_CONTROL_MESSAGE;
    rtmp_msg->timestamp_ = 0;
    rtmp_msg->message_type_id_ = RTMP_CHUNK_MESSAGE_TYPE_ACKNOWLEDGEMENT;
    rtmp_msg->message_stream_id_ = RTMP_MESSAGE_ID_PROTOCOL_CONTROL;
    // chunk_size_
    auto unuse_data = rtmp_msg->get_unuse_data();
    *(uint32_t*)(unuse_data.data()) = htonl(acknowledge_);
    rtmp_msg->inc_used_bytes(sizeof(acknowledge_));
    return rtmp_msg;
}