#include "rtmp_create_stream_message.hpp"
using namespace mms;
RtmpCreateStreamMessage::RtmpCreateStreamMessage(int32_t transaction_id) {
    transaction_id_.set_value(transaction_id);
    command_name_.set_value("createStream");
}

RtmpCreateStreamMessage::RtmpCreateStreamMessage() {

}

RtmpCreateStreamMessage::~RtmpCreateStreamMessage() {

}

int32_t RtmpCreateStreamMessage::decode(std::shared_ptr<RtmpMessage> rtmp_msg) {
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
        consumed = null_.decode(payload, len);
        if (consumed < 0) {
            return -4;
        }
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;
    return pos;
}

int32_t RtmpCreateStreamMessage::size() const {
    int32_t size = 0;
    size += command_name_.size();
    size += transaction_id_.size();
    if (command_obj_.get_property_count() <= 0) {
        Amf0Null null;
        size += null.size();
    } else {
        size += command_obj_.size();
    }
    
    return size;
}

std::shared_ptr<RtmpMessage> RtmpCreateStreamMessage::encode() const{
    //todo implement this method
    
    auto need_size = size();
    std::shared_ptr<RtmpMessage> rtmp_msg = std::make_shared<RtmpMessage>(need_size);
    rtmp_msg->chunk_stream_id_ = RTMP_CHUNK_ID_COMMAND_MESSAGE;//RTMP_CHUNK_ID_PROTOCOL_CONTROL_MESSAGE;
    rtmp_msg->timestamp_ = 0;
    rtmp_msg->message_type_id_ = RTMP_MESSAGE_TYPE_AMF0_COMMAND;
    rtmp_msg->message_stream_id_ = RTMP_MESSAGE_ID_PROTOCOL_CONTROL;
    // window ack_size
    auto unuse_data = rtmp_msg->get_unuse_data();
    uint8_t * payload = (uint8_t*)unuse_data.data();
    int32_t left_size = need_size;
    int32_t consumed = command_name_.encode(payload, left_size);
    if (consumed <= 0) {
        return nullptr;
    }

    payload += consumed;
    left_size -= consumed;
    consumed = transaction_id_.encode(payload, left_size);
    if (consumed <= 0) {
        return nullptr;
    }

    payload += consumed;
    left_size -= consumed;
    if (command_obj_.get_property_count() > 0) {
        consumed = command_obj_.encode(payload, left_size);
        if (consumed <= 0) {
            return nullptr;
        }
    } else {
        Amf0Null null;
        consumed = null.encode(payload, left_size);
        if (consumed <= 0) {
            return nullptr;
        }
    }
    
    payload += consumed;
    left_size -= consumed;
    // rtmp_msg->payload_size_ = need_size;
    rtmp_msg->inc_used_bytes(need_size);
    return rtmp_msg;
}
