#include "rtmp_release_stream_message.hpp"
using namespace mms;
RtmpReleaseStreamMessage::RtmpReleaseStreamMessage() {

}

RtmpReleaseStreamMessage::RtmpReleaseStreamMessage(int32_t transaction_id, const std::string & stream_name) {
    command_name_.set_value("releaseStream");
    transaction_id_.set_value(transaction_id);
    stream_name_.set_value(stream_name);
}

RtmpReleaseStreamMessage::~RtmpReleaseStreamMessage() {

}

int32_t RtmpReleaseStreamMessage::size() const {
    int32_t size = 0;
    size += command_name_.size();
    size += transaction_id_.size();
    size += null_.size();
    size += stream_name_.size();
    return size;
}

int32_t RtmpReleaseStreamMessage::decode(std::shared_ptr<RtmpMessage> rtmp_msg) {
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

    consumed = null_.decode(payload, len);
    if (consumed < 0) {
        return -3;
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;

    consumed = stream_name_.decode(payload, len);
    if (consumed < 0) {
        return -4;
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;
    return pos;
}

std::shared_ptr<RtmpMessage> RtmpReleaseStreamMessage::encode() const {
    auto need_size = size();
    std::shared_ptr<RtmpMessage> rtmp_msg = std::make_shared<RtmpMessage>(need_size);
    rtmp_msg->chunk_stream_id_ = RTMP_CHUNK_ID_COMMAND_MESSAGE;//RTMP_CHUNK_ID_PROTOCOL_CONTROL_MESSAGE;
    rtmp_msg->timestamp_ = 0;
    rtmp_msg->message_type_id_ = RTMP_MESSAGE_TYPE_AMF0_COMMAND;
    rtmp_msg->message_stream_id_ = RTMP_MESSAGE_ID_PROTOCOL_CONTROL;

    int32_t consumed = 0;
    int32_t pos = 0;
    auto unuse_data = rtmp_msg->get_unuse_data();
    uint8_t *payload = (uint8_t*)unuse_data.data();
    int32_t len = need_size;
    consumed = command_name_.encode(payload, len);
    if (consumed < 0) {
        return nullptr;
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;

    consumed = transaction_id_.encode(payload, len);
    if(consumed < 0) {
        return nullptr;
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;

    consumed = null_.encode(payload, len);
    if (consumed < 0) {
        return nullptr;
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;
    
    consumed = stream_name_.encode(payload, len);
    if(consumed < 0) {
        return nullptr;
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;
    // rtmp_msg->payload_size_ = pos;
    rtmp_msg->inc_used_bytes(pos);
    return rtmp_msg;
}
