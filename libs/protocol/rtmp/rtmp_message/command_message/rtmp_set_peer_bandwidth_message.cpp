#include <arpa/inet.h>
#include "rtmp_define.hpp"
#include "rtmp_set_peer_bandwidth_message.hpp"
using namespace mms;

int32_t RtmpSetPeerBandwidthMessage::decode(std::shared_ptr<RtmpMessage> rtmp_msg) {
    auto using_data = rtmp_msg->get_using_data();
    uint8_t * payload = (uint8_t*)using_data.data();
    int32_t len = using_data.size();
    if (len < 5) {
        return -1;
    }
    // todo modify to ntohl
    ack_window_size_ = ntohl(*(uint32_t*)payload);
    limit_type_ = payload[4];
    return 5;
}

std::shared_ptr<RtmpMessage> RtmpSetPeerBandwidthMessage::encode() const {
    std::shared_ptr<RtmpMessage> rtmp_msg = std::make_shared<RtmpMessage>(sizeof(ack_window_size_) + sizeof(limit_type_));
    rtmp_msg->chunk_stream_id_ = RTMP_CHUNK_ID_PROTOCOL_CONTROL_MESSAGE;
    rtmp_msg->timestamp_ = 0;
    rtmp_msg->message_type_id_ = RTMP_CHUNK_MESSAGE_TYPE_SET_PEER_BANDWIDTH;
    rtmp_msg->message_stream_id_ = RTMP_MESSAGE_ID_PROTOCOL_CONTROL;
    // window ack_size
    auto unuse_data = rtmp_msg->get_unuse_data();
    uint8_t * payload = (uint8_t*)unuse_data.data();
    *(uint32_t*)payload = htonl(ack_window_size_);
    // limit type
    payload[4] = limit_type_;
    rtmp_msg->inc_used_bytes(sizeof(ack_window_size_) + sizeof(limit_type_));
    // rtmp_msg->payload_size_ = sizeof(ack_window_size_) + sizeof(limit_type_);

    return rtmp_msg;
}