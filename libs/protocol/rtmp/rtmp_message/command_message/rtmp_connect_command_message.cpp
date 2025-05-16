#include "rtmp_connect_command_message.hpp"
using namespace mms;
RtmpConnectCommandMessage::RtmpConnectCommandMessage() {

}

RtmpConnectCommandMessage::RtmpConnectCommandMessage(int32_t transaction_id, const std::string & tc_url, const std::string & page_url, const std::string & swf_url, const std::string & app) {
    tc_url_ = tc_url;
    command_name_.set_value("connect");
    transaction_id_.set_value(transaction_id);
    page_url_ = page_url;
    swf_url_ = swf_url;
    app_ = app;
    command_object_.set_item_value("tcUrl", tc_url_);
    command_object_.set_item_value("pageUrl", page_url_);
    command_object_.set_item_value("swfUrl", swf_url_);
    command_object_.set_item_value("app", app_);
}

RtmpConnectCommandMessage::~RtmpConnectCommandMessage() {

}

int32_t RtmpConnectCommandMessage::decode(std::shared_ptr<RtmpMessage> rtmp_msg) {
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

    consumed = command_object_.decode(payload, len);
    if (consumed < 0) {
        return -3;
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;
    
    if (len > 0) {
        Amf0Object opt;
        consumed = opt.decode(payload, len);
        if (consumed < 0) {
            return -4;
        }
        optional_user_args_ = opt;
        pos += consumed;
        payload += consumed;
        len -= consumed;
    }
    
    auto tcUrl = command_object_.get_property<Amf0String>("tcUrl");
    if (!tcUrl) {
        return -5;
    }
    tc_url_ = *tcUrl;
    auto pageUrl = command_object_.get_property<Amf0String>("pageUrl");
    if (pageUrl) {
        page_url_ = *pageUrl;
    }
    
    auto swfUrl = command_object_.get_property<Amf0String>("swfUrl");
    if (swfUrl) {
        swf_url_ = *swfUrl;
    }

    auto app = command_object_.get_property<Amf0String>("app");
    if (app) {
        app_ = *app;
    }
    
    auto objectEncoding = command_object_.get_property<Amf0Number>("objectEncoding");
    if (objectEncoding) {
        object_encoding_ = *objectEncoding;
    }
 
    return pos;
}

int32_t RtmpConnectCommandMessage::size() const {
    int32_t size = 0;
    size += command_name_.size();
    size += transaction_id_.size();
    size += command_object_.size();
    if (optional_user_args_) {
        size += optional_user_args_.value().size();
    }
    return size;
}

std::shared_ptr<RtmpMessage> RtmpConnectCommandMessage::encode() const {
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
    consumed = command_object_.encode(payload, left_size);
    if (consumed <= 0) {
        return nullptr;
    }

    payload += consumed;
    left_size -= consumed;
    if (optional_user_args_) {
        consumed = optional_user_args_.value().encode(payload, left_size);
        if (consumed <= 0) {
            return nullptr;
        }
    }
    
    // rtmp_msg->payload_size_ = need_size;
    rtmp_msg->inc_used_bytes(need_size);
    return rtmp_msg;
}
