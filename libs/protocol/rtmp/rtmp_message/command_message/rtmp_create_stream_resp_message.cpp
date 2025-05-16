#include "rtmp_create_stream_resp_message.hpp"
using namespace mms;

RtmpCreateStreamRespMessage::RtmpCreateStreamRespMessage(int32_t transaction_id) {
    command_name_.set_value("_result");
    transaction_id_.set_value(transaction_id);
}

double RtmpCreateStreamRespMessage::get_stream_id() {
    return stream_id_.get_value();
}

int32_t RtmpCreateStreamRespMessage::decode(std::shared_ptr<RtmpMessage> rtmp_msg) {
    auto using_data = rtmp_msg->get_using_data();
    const uint8_t *payload = (const uint8_t*)using_data.data();
    const uint8_t *data_start = payload;
    int32_t len = using_data.size();//rtmp_msg->payload_size_;
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

    consumed = command_obj_.decode(payload, len);
    if (consumed <= 0) {
        return -3;
    }
    payload += consumed;
    len -= consumed;

    consumed = stream_id_.decode(payload, len);
    if (consumed <= 0) {
        return -4;
    }
    payload += consumed;
    len -= consumed;
    return payload - data_start;
}

std::shared_ptr<RtmpMessage> RtmpCreateStreamRespMessage::encode() const {
    size_t s = 0;
    s += command_name_.size();
    s += transaction_id_.size();
    s += command_obj_.size();
    s += stream_id_.size();

    std::shared_ptr<RtmpMessage> rtmp_msg = std::make_shared<RtmpMessage>(s);
    rtmp_msg->chunk_stream_id_ = RTMP_CHUNK_ID_PROTOCOL_CONTROL_MESSAGE;
    rtmp_msg->timestamp_ = 0;
    rtmp_msg->message_type_id_ = RTMP_MESSAGE_TYPE_AMF0_COMMAND;
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

    consumed = transaction_id_.encode(payload, len);
    if (consumed < 0) {
        return nullptr;
    }
    payload += consumed;
    len -= consumed;

    consumed = command_obj_.encode(payload, len);
    if (consumed < 0) {
        return nullptr;
    }
    payload += consumed;
    len -= consumed;

    consumed = stream_id_.encode(payload, len);
    if (consumed < 0) {
        return nullptr;
    }
    payload += consumed;
    len -= consumed;
    // rtmp_msg->payload_size_ = s;
    rtmp_msg->inc_used_bytes(s);
    return rtmp_msg;
}