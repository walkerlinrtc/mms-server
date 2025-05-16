#include <arpa/inet.h>
#include "rtmp_window_acknowledge_size_message.hpp"
using namespace mms;

RtmpWindowAcknwledgeSizeMessage::RtmpWindowAcknwledgeSizeMessage(size_t s) : acknowledge_windows_size_(s) {

}

RtmpWindowAcknwledgeSizeMessage::RtmpWindowAcknwledgeSizeMessage() {

}

int32_t RtmpWindowAcknwledgeSizeMessage::decode(std::shared_ptr<RtmpMessage> rtmp_msg) {
    auto using_data = rtmp_msg->get_using_data();
    uint8_t * payload = (uint8_t*)using_data.data();
    int32_t len = using_data.size();
    if (len < 4) {
        return -1;
    }
    acknowledge_windows_size_ = ntohl(*(uint32_t*)payload);
    return 4;
}

std::shared_ptr<RtmpMessage> RtmpWindowAcknwledgeSizeMessage::encode() const {
    std::shared_ptr<RtmpMessage> rtmp_msg = std::make_shared<RtmpMessage>(4);
    rtmp_msg->chunk_stream_id_ = RTMP_CHUNK_ID_PROTOCOL_CONTROL_MESSAGE;
    rtmp_msg->timestamp_ = 0;
    rtmp_msg->message_type_id_ = RTMP_CHUNK_MESSAGE_TYPE_WINDOW_ACK_SIZE;
    rtmp_msg->message_stream_id_ = RTMP_MESSAGE_ID_PROTOCOL_CONTROL;
    // chunk_size_
    auto unuse_data = rtmp_msg->get_unuse_data();
    *(uint32_t*)(unuse_data.data()) = htonl(acknowledge_windows_size_);
    rtmp_msg->inc_used_bytes(sizeof(acknowledge_windows_size_));
    return rtmp_msg;
}