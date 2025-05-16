#include "rtmp_connect_resp_message.hpp"
#include "rtmp_connect_command_message.hpp"

using namespace mms;

RtmpConnectRespMessage::RtmpConnectRespMessage(const RtmpConnectCommandMessage & conn_msg, const std::string & name) {
    command_name_.set_value(name);
    transaction_id_.set_value(conn_msg.transaction_id_.get_value());
}

RtmpConnectRespMessage::RtmpConnectRespMessage() {
        
}

RtmpConnectRespMessage::~RtmpConnectRespMessage() {

}

int32_t RtmpConnectRespMessage::decode(std::shared_ptr<RtmpMessage> rtmp_msg) {
    auto using_data = rtmp_msg->get_using_data();
    const uint8_t *payload = (const uint8_t*)using_data.data();
    const uint8_t *data_start = payload;
    int32_t len = using_data.size();
    int32_t consumed = command_name_.decode(payload, len);
    if (consumed <= 0) {
        return -1;
    }
    payload += consumed;
    len -= consumed;

    consumed = transaction_id_.decode(payload, len);
    if (consumed <= 0) {
        return -2;
    }
    payload += consumed;
    len -= consumed;

    consumed = props_.decode(payload, len);
    if (consumed <= 0) {
        return -3;
    }
    payload += consumed;
    len -= consumed;

    consumed = info_.decode(payload, len);
    if (consumed <= 0) {
        return -4;
    }
    payload += consumed;
    len -= consumed;
    return payload - data_start;
}

std::shared_ptr<RtmpMessage> RtmpConnectRespMessage::encode() const {
    size_t s = 0;
    s += command_name_.size();
    s += transaction_id_.size();
    s += props_.size();
    s += info_.size();

    std::shared_ptr<RtmpMessage> rtmp_msg = std::make_shared<RtmpMessage>(s);
    rtmp_msg->chunk_stream_id_ = RTMP_CHUNK_ID_PROTOCOL_CONTROL_MESSAGE;
    rtmp_msg->timestamp_ = 0;
    rtmp_msg->message_type_id_ = RTMP_MESSAGE_TYPE_AMF0_COMMAND;
    rtmp_msg->message_stream_id_ = RTMP_MESSAGE_ID_PROTOCOL_CONTROL;
    auto unuse_data = rtmp_msg->get_unuse_data();
    uint8_t * payload = (uint8_t*)unuse_data.data();
    int32_t len = s;
    int32_t consumed = command_name_.encode(payload, len);
    if (consumed < 0) {
        return nullptr;
    }
    payload += consumed;
    len -= consumed;

    consumed = transaction_id_.encode(payload, len);
    if (consumed < 0) {
        return nullptr;
    }
    payload += consumed;
    len -= consumed;

    consumed = props_.encode(payload, len);
    if (consumed < 0) {
        return nullptr;
    }
    payload += consumed;
    len -= consumed;

    consumed = info_.encode(payload, len);
    if (consumed < 0) {
        return nullptr;
    }
    payload += consumed;
    len -= consumed;
    // rtmp_msg->payload_size_ = s;
    rtmp_msg->inc_used_bytes(s);
    return rtmp_msg;
}