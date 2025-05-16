#include "rtmp_play_message.hpp"
using namespace mms;
RtmpPlayMessage::RtmpPlayMessage() {

}

RtmpPlayMessage::RtmpPlayMessage(int32_t transaction_id) {
    Amf0Number start;
    start.set_value(-2);
    start_ = start;

    Amf0Number duration;
    duration.set_value(-1);
    duration_ = duration;

    Amf0Boolean reset;
    reset.set_value(true);
    reset_ = reset;

    command_name_.set_value("play");
    transaction_id_.set_value(transaction_id);
}

RtmpPlayMessage::RtmpPlayMessage(int32_t transaction_id, const std::string & stream_name) {
    command_name_.set_value("play");
    stream_name_.set_value(stream_name);
    Amf0Number start;
    start.set_value(-2000);
    start_ = start;
    transaction_id_.set_value(transaction_id);
}

RtmpPlayMessage::~RtmpPlayMessage() {

}

int32_t RtmpPlayMessage::decode(std::shared_ptr<RtmpMessage> rtmp_msg) {
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
    if(consumed < 0) {
        return -5;
    }
    pos += consumed;
    payload += consumed;
    len -= consumed;

    if (len > 0) {
        Amf0Number start;
        consumed = start.decode(payload, len);
        if(consumed < 0) {
            return -6;
        }

        pos += consumed;
        payload += consumed;
        len -= consumed;

        start_ = start;
        start_pts_ = start.get_value();
    }

    if (len > 0) {
        Amf0Number duration;
        consumed = duration.decode(payload, len);
        if(consumed < 0) {
            return -7;
        }

        pos += consumed;
        payload += consumed;
        len -= consumed;
        
        duration_ = duration;
        duration_ms_ = duration_.value().get_value();
    }

    if (len > 0) {
        Amf0Boolean reset;
        consumed = reset.decode(payload, len);
        if(consumed < 0) {
            return -8;
        }

        pos += consumed;
        payload += consumed;
        len -= consumed;
        
        reset_ = reset;
        reset_play_ = reset_.value().get_value();
    }
    
    return pos;
}

const std::string & RtmpPlayMessage::stream_name() const {
    return stream_name_.get_value();
}

int32_t RtmpPlayMessage::size() const {
    int32_t size = 0;
    size += command_name_.size();
    size += transaction_id_.size();
    size += null_.size();
    size += stream_name_.size();
    if (start_) {
        size += start_.value().size();
    }
    
    if (duration_) {
        size += duration_.value().size();
    }
    
    if (reset_) {
        size += reset_.value().size();
    }
    
    return size;
}

std::shared_ptr<RtmpMessage> RtmpPlayMessage::encode() const {
    //todo implement this method
    
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

    if (start_) {
        consumed = start_.value().encode(payload, len);
        if(consumed < 0) {
            return nullptr;
        }

        pos += consumed;
        payload += consumed;
        len -= consumed;
    }

    if (duration_) {
        consumed = duration_.value().encode(payload, len);
        if(consumed < 0) {
            return nullptr;
        }

        pos += consumed;
        payload += consumed;
        len -= consumed;
    }

    if (reset_) {
        consumed = reset_.value().encode(payload, len);
        if(consumed < 0) {
            return nullptr;
        }

        pos += consumed;
        payload += consumed;
        len -= consumed;
    }
    // rtmp_msg->payload_size_ = pos;
    rtmp_msg->inc_used_bytes(pos);
    return rtmp_msg;
}
