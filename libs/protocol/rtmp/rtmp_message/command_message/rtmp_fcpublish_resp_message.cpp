#include "rtmp_fcpublish_message.hpp"
#include "rtmp_fcpublish_resp_message.hpp"

using namespace mms;

RtmpFCPublishRespMessage::RtmpFCPublishRespMessage(const RtmpFCPublishMessage & rel_msg, const std::string & name) {
    command_name_.set_value(name);
    transaction_id_.set_value(rel_msg.transaction_id_.get_value());
}

RtmpFCPublishRespMessage::RtmpFCPublishRespMessage() {
    
}

RtmpFCPublishRespMessage::~RtmpFCPublishRespMessage() {

}

int32_t RtmpFCPublishRespMessage::decode(std::shared_ptr<RtmpMessage> rtmp_msg) {
    int32_t consumed = 0;
    int32_t pos = 0;
    auto using_data = rtmp_msg->get_using_data();
    const uint8_t *payload = (const uint8_t*)using_data.data();
    int32_t len = using_data.size();
    consumed = command_name_.decode(payload, len);
    if (consumed < 0) {
        return -1;
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;

    consumed = transaction_id_.decode(payload, len);
    if(consumed < 0) {
        return -2;
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;

    consumed = command_obj_.decode(payload, len);
    if (consumed < 0) {
        return -3;
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;

    consumed = udef_.decode(payload, len);
    if (consumed < 0) {
        return -3;
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;
    return pos;
}

std::shared_ptr<RtmpMessage> RtmpFCPublishRespMessage::encode() const {
    size_t s = 0;
    s += command_name_.size();
    s += transaction_id_.size();
    s += command_obj_.size();
    s += udef_.size();

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

    consumed = udef_.encode(payload, len);
    if (consumed < 0) {
        return nullptr;
    }
    payload += consumed;
    len -= consumed;
    rtmp_msg->inc_used_bytes(s);
    return rtmp_msg;
}
