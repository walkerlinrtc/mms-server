#include "rtmp_define.hpp"
using namespace mms;

RtmpMessage::RtmpMessage(int32_t payload_size) : Packet(PACKET_RTMP, payload_size) {
}

RtmpMessage::~RtmpMessage() {
}

uint8_t RtmpMessage::get_message_type() {
    return message_type_id_;
}