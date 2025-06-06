#include <list>
#include <variant>

#include "base/network/socket_interface.hpp"
#include "rtmp_chunk_protocol.hpp"
#include "protocol/rtmp/rtmp_message/chunk_message/rtmp_window_acknowledge_size_message.hpp"
#include "protocol/rtmp/rtmp_message/chunk_message/rtmp_acknowledge_message.hpp"
#include "spdlog/spdlog.h"

using namespace mms;
using namespace boost::asio::experimental::awaitable_operators;

constexpr uint32_t MAX_BUFFER_BYTES = 2*1024*1024;

RtmpChunkProtocol::RtmpChunkProtocol(std::shared_ptr<SocketInterface> conn):conn_(conn) {
    recv_buffer_ = new uint8_t[MAX_BUFFER_BYTES];
    recv_len_ = 0;

    for (uint32_t cid = 0; cid < 256; cid++) {
        recv_chunk_cache_[cid] = std::make_shared<RtmpChunk>();
    }

    for (int i = 0; i < 100; i++) {
        chunk_headers_.push_back(std::make_unique<char[]>(40));
    }

    send_sv_bufs_.reserve(200);
}

RtmpChunkProtocol::~RtmpChunkProtocol() {
    if (recv_buffer_) {
        delete[] recv_buffer_;
        recv_buffer_ = nullptr;
    }
}

// 循环接收消息

boost::asio::awaitable<int32_t> RtmpChunkProtocol::cycle_recv_rtmp_message(const std::function<boost::asio::awaitable<bool>(std::shared_ptr<RtmpMessage>)> & recv_handler) {
    recv_handler_ = recv_handler;
    while (1) {
        if (MAX_BUFFER_BYTES - recv_len_ <= 0) {
            spdlog::error("no enough buffer for rtmp");
            co_return -3;
        }

        auto s = co_await conn_->recv_some(recv_buffer_ + recv_len_, MAX_BUFFER_BYTES - recv_len_);
        if (s < 0) {
            co_return s;
        }
        recv_len_ += s;

        while (1) {
            auto consumed = co_await process_recv_buffer();
            if (consumed == 0) {// 0 表示不够数据
                break;
            } else if (consumed < 0) {
                co_return -2;
            }

            // 解析完成，移动buffer
            auto left_size = recv_len_ - consumed;
            memmove(recv_buffer_, recv_buffer_ + consumed, left_size);
            recv_len_ = left_size;
        }
    }
    co_return 0;
}

boost::asio::awaitable<bool> RtmpChunkProtocol::send_rtmp_messages(const std::vector<std::shared_ptr<RtmpMessage>> & rtmp_msgs) {
    send_sv_bufs_.clear();
    int curr_chunk_header = 0;
    for (auto rtmp_msg : rtmp_msgs) {
        int32_t left_size = rtmp_msg->get_using_data().size();
        size_t cur_pos = 0;
        uint8_t fmt = RTMP_CHUNK_FMT_TYPE0;
        while (left_size > 0) {
            // 判断fmt类型
            size_t buf_pos = 0;
            auto prev_chunk = send_chunk_streams_[rtmp_msg->chunk_stream_id_];
            if (prev_chunk) {
                if (prev_chunk->chunk_message_header_.message_stream_id_ == rtmp_msg->message_stream_id_ &&
                    prev_chunk->chunk_message_header_.message_type_id_ == rtmp_msg->message_type_id_ &&
                    prev_chunk->chunk_message_header_.message_length_ == (int32_t)rtmp_msg->get_using_data().size() &&
                    prev_chunk->chunk_message_header_.timestamp_delta_ == (rtmp_msg->timestamp_ - prev_chunk->chunk_message_header_.timestamp_)) {// fmt3
                    fmt = RTMP_CHUNK_FMT_TYPE3;
                } else if (prev_chunk->chunk_message_header_.message_stream_id_ == rtmp_msg->message_stream_id_ &&
                           prev_chunk->chunk_message_header_.message_type_id_ == rtmp_msg->message_type_id_ &&
                           prev_chunk->chunk_message_header_.message_length_ == (int32_t)rtmp_msg->get_using_data().size()) {//fmt2
                    fmt = RTMP_CHUNK_FMT_TYPE2;            
                } else if (prev_chunk->chunk_message_header_.message_stream_id_ == rtmp_msg->message_stream_id_) {//fmt1
                    fmt = RTMP_CHUNK_FMT_TYPE1;
                } else {
                    fmt = RTMP_CHUNK_FMT_TYPE0;
                }
            } else {
                fmt = RTMP_CHUNK_FMT_TYPE0;
            }

            //  +--------------+----------------+--------------------+--------------+
            //  | Basic Header | Message Header | Extended Timestamp | Chunk Data |
            //  +--------------+----------------+--------------------+--------------+
            // 发送basic header
            std::shared_ptr<RtmpChunk> chunk = std::make_shared<RtmpChunk>();
            chunk->chunk_message_header_.message_stream_id_ = rtmp_msg->message_stream_id_;
            chunk->chunk_message_header_.message_type_id_ = rtmp_msg->message_type_id_;
            chunk->chunk_message_header_.message_length_ = rtmp_msg->get_using_data().size();
            chunk->chunk_message_header_.timestamp_ = rtmp_msg->timestamp_;
            if (prev_chunk) {
                chunk->chunk_message_header_.timestamp_delta_ = chunk->chunk_message_header_.timestamp_ - prev_chunk->chunk_message_header_.timestamp_;
            }
            
            chunk->rtmp_message_ = rtmp_msg;
            // Chunk stream IDs 2-63 can be encoded in the 1-byte version of this field.
            if (rtmp_msg->chunk_stream_id_ >= 2 && rtmp_msg->chunk_stream_id_ <= 63) {
                uint8_t d = ((fmt&0x03)<<6) | (rtmp_msg->chunk_stream_id_&0x3f);
                // memcpy(send_buffer_ + buf_pos, &d, sizeof(d));
                memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos, &d, sizeof(d));
                buf_pos++;
            } else if (rtmp_msg->chunk_stream_id_ >= 64 && rtmp_msg->chunk_stream_id_ <= 319) {
                // uint8_t buf[2];
                uint8_t *p = (uint8_t*)chunk_headers_[curr_chunk_header].get() + buf_pos;
                p[0] = ((fmt&0x03)<<6) | 0x00;
                p[1] = (rtmp_msg->chunk_stream_id_ - 64) & 0xff;
                // buf[0] = ((fmt&0x03)<<6) | 0x00;
                // buf[1] = (rtmp_msg->chunk_stream_id_ - 64) & 0xff;
                // memcpy(send_buffer_ + buf_pos, buf, sizeof(buf));
                // memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos, buf, sizeof(buf));
                buf_pos += 2;
            } else if (rtmp_msg->chunk_stream_id_ >= 64 && rtmp_msg->chunk_stream_id_ <= 65599) {
                // uint8_t buf[3];
                uint8_t *p = (uint8_t*)chunk_headers_[curr_chunk_header].get() + buf_pos;
                p[0] = ((fmt&0x03)<<6) | 0x01;
                auto csid = rtmp_msg->chunk_stream_id_ - 64;
                p[1] = (csid%256) & 0xff;
                p[2] = (csid/256) & 0xff;
                // memcpy(send_buffer_ + buf_pos, buf, sizeof(buf));
                // memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos, buf, sizeof(buf));
                buf_pos += 3;
            }
            // 发送message header
            int this_chunk_payload_size = std::min(out_chunk_size_, left_size);
            bool has_extend_timestamp = false;
            if (fmt == RTMP_CHUNK_FMT_TYPE0) {
                if (rtmp_msg->timestamp_ >= 0xffffff) {
                    has_extend_timestamp = true;
                }
                
                if (!has_extend_timestamp) {
                    uint32_t t = htonl(rtmp_msg->timestamp_&0xffffff);
                    // memcpy(send_buffer_ + buf_pos, (uint8_t*)&t+1, 3);
                    memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos, (uint8_t*)&t+1, 3);
                } else {
                    uint32_t t = 0xffffffff;
                    // memcpy(send_buffer_ + buf_pos, (uint8_t*)&t+1, 3);
                    memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos, (uint8_t*)&t+1, 3);
                }
                
                buf_pos += 3;

                int32_t t = htonl(rtmp_msg->get_using_data().size());
                // memcpy(send_buffer_ + buf_pos, (uint8_t*)&t+1, 3);
                memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos, (uint8_t*)&t+1, 3);
                buf_pos += 3;
                // memcpy(send_buffer_ + buf_pos, &rtmp_msg->message_type_id_, 1);
                memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos, &rtmp_msg->message_type_id_, 1);
                buf_pos += 1;

                t = htonl(rtmp_msg->message_stream_id_);
                // memcpy(send_buffer_ + buf_pos, &t, 4);
                memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos, &t, 4);
                buf_pos += 4;
                if (has_extend_timestamp) {
                    uint32_t t = htonl(rtmp_msg->timestamp_);
                    // memcpy(send_buffer_ + buf_pos, (uint8_t*)&t, 4);
                    memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos,(uint8_t*)&t, 4);
                    buf_pos += 4;
                }
            } else if (fmt == RTMP_CHUNK_FMT_TYPE1) {
                uint32_t timestamp_delta = rtmp_msg->timestamp_ - prev_chunk->rtmp_message_->timestamp_;
                if (timestamp_delta >= 0xffffff) {
                    has_extend_timestamp = true;
                }

                if (!has_extend_timestamp) {
                    uint32_t t = htonl(timestamp_delta);
                    // memcpy(send_buffer_ + buf_pos, (uint8_t*)&t + 1, 3);
                    memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos, (uint8_t*)&t + 1, 3);
                } else {
                    uint32_t t = 0xffffffff;
                    memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos, (uint8_t*)&t + 1, 3);
                }
                
                buf_pos += 3;

                int32_t t = htonl(rtmp_msg->get_using_data().size());
                // memcpy(send_buffer_ + buf_pos, (uint8_t*)&t + 1, 3);
                memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos, (uint8_t*)&t + 1, 3);
                buf_pos += 3;

                // memcpy(send_buffer_ + buf_pos, &rtmp_msg->message_type_id_, 1);
                memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos, &rtmp_msg->message_type_id_, 1);
                buf_pos += 1;
                if (has_extend_timestamp) {
                    uint32_t t = htonl(timestamp_delta);
                    // memcpy(send_buffer_ + buf_pos, (uint8_t*)&t + 1, 3);
                    memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos, (uint8_t*)&t, 4);
                    buf_pos += 4;
                }
            } else if (fmt == RTMP_CHUNK_FMT_TYPE2) {
                uint32_t timestamp_delta = rtmp_msg->timestamp_ - prev_chunk->rtmp_message_->timestamp_;
                if (timestamp_delta >= 0xffffff) {
                    has_extend_timestamp = true;
                }

                if (!has_extend_timestamp) {
                    uint32_t t = htonl(timestamp_delta);
                    // memcpy(send_buffer_ + buf_pos, (uint8_t*)&t + 1, 3);
                    memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos,(uint8_t*)&t + 1, 3);
                } else {
                    uint32_t t = 0xffffffff;
                    // memcpy(send_buffer_ + buf_pos, (uint8_t*)&t + 1, 3);
                    memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos,(uint8_t*)&t + 1, 3);
                }
                buf_pos += 3;

                if (has_extend_timestamp) {
                    uint32_t t = htonl(timestamp_delta);
                    // memcpy(send_buffer_ + buf_pos, (uint8_t*)&t + 1, 3);
                    memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos,(uint8_t*)&t, 4);
                    buf_pos += 4;
                }
            } else if (fmt == RTMP_CHUNK_FMT_TYPE3) {// no header
                uint32_t timestamp_delta = rtmp_msg->timestamp_ - prev_chunk->rtmp_message_->timestamp_;
                if (timestamp_delta >= 0xffffff) {
                    has_extend_timestamp = true;
                }

                if (has_extend_timestamp) {
                    uint32_t t = htonl(timestamp_delta);
                    // memcpy(send_buffer_ + buf_pos, (uint8_t*)&t + 1, 3);
                    memcpy(chunk_headers_[curr_chunk_header].get() + buf_pos,(uint8_t*)&t, 4);
                    buf_pos += 4;
                }
            }

            send_sv_bufs_.push_back(boost::asio::const_buffer((char*)chunk_headers_[curr_chunk_header].get(), buf_pos));
            // 发送chunk data
            send_sv_bufs_.push_back(boost::asio::const_buffer((char*)rtmp_msg->get_using_data().data() + cur_pos, this_chunk_payload_size));
            curr_chunk_header++;
            left_size -= this_chunk_payload_size;
            cur_pos += this_chunk_payload_size;
            // 发送结束，记录本次发送的chunk
            send_chunk_streams_[rtmp_msg->chunk_stream_id_] = chunk;
        }
    }

    if (!(co_await conn_->send(send_sv_bufs_))) {
        co_return false;
    }
    
    co_return true;
}

boost::asio::awaitable<int32_t> RtmpChunkProtocol::process_recv_buffer()
{
    int32_t left_size = recv_len_;
    int32_t buf_pos = 0;
    uint8_t d;
    if (left_size < 1) {
        co_return 0;
    }
    d = recv_buffer_[buf_pos];
    buf_pos++;
    left_size--;

    int32_t csid = (int32_t)(d & 0x3f);
    int8_t fmt = (d >> 6) & 0x03;
    if (csid == 0) {
        if (left_size < 1) {
            co_return 0;
        }
        csid += 64;
        csid += (int32_t)recv_buffer_[buf_pos];
        buf_pos++;
        left_size--;
    } else if(csid == 1) {
        if (left_size < 2) {
            co_return 0;
        }

        csid = 64;
        csid += (int32_t)(recv_buffer_[buf_pos]);
        csid += (int32_t)(recv_buffer_[buf_pos+1])*256;
        buf_pos += 2;
        left_size -= 2;
    }
    // read chunk message header
    std::shared_ptr<RtmpChunk> prev_chunk;
    auto csid_it = recv_chunk_streams_.find(csid);
    if (csid_it != recv_chunk_streams_.end()) {
        prev_chunk = csid_it->second;
    }
    // this chunk info
    // 优先从cache中获取chunk
    std::shared_ptr<RtmpChunk> chunk;
    auto cache_csid_it = recv_chunk_cache_.find(csid);
    if (cache_csid_it != recv_chunk_cache_.end()) {
        chunk = cache_csid_it->second;
        recv_chunk_cache_.erase(cache_csid_it);
    }

    if (!chunk) {
        chunk = std::make_shared<RtmpChunk>();
    }

    chunk->chunk_message_header_.fmt_ = fmt;
    if (fmt == RTMP_CHUNK_FMT_TYPE0) {
        if (left_size < 3) {
            co_return 0;
        }

        uint8_t *p = (uint8_t*)&chunk->chunk_message_header_.timestamp_;
        p[0] = recv_buffer_[buf_pos + 2];
        p[1] = recv_buffer_[buf_pos + 1];
        p[2] = recv_buffer_[buf_pos + 0];
        buf_pos += 3;
        left_size -= 3;

        if (left_size < 3) {
            co_return 0;
        }

        p = (uint8_t*)&chunk->chunk_message_header_.message_length_;
        p[0] = recv_buffer_[buf_pos + 2];
        p[1] = recv_buffer_[buf_pos + 1];
        p[2] = recv_buffer_[buf_pos + 0];
        buf_pos += 3;
        left_size -= 3;

        if (left_size < 1) {
            co_return 0;
        }
        chunk->chunk_message_header_.message_type_id_ = recv_buffer_[buf_pos];
        buf_pos++;
        left_size--;

        if (left_size < 4) {
            co_return 0;
        }
        p = (uint8_t*)&chunk->chunk_message_header_.message_stream_id_;
        p[0] = recv_buffer_[buf_pos + 3];
        p[1] = recv_buffer_[buf_pos + 2];
        p[2] = recv_buffer_[buf_pos + 1];
        p[3] = recv_buffer_[buf_pos + 0];
        buf_pos += 4;
        left_size -= 4;

        if (chunk->chunk_message_header_.timestamp_ == 0x00ffffff) {
            if (left_size < 4) {
                co_return 0;
            }

            p = (uint8_t*)&chunk->chunk_message_header_.timestamp_;
            p[0] = recv_buffer_[buf_pos + 3];
            p[1] = recv_buffer_[buf_pos + 2];
            p[2] = recv_buffer_[buf_pos + 1];
            p[3] = recv_buffer_[buf_pos + 0];
            buf_pos += 4;
            left_size -= 4;
        }
    } else if (fmt == RTMP_CHUNK_FMT_TYPE1) {//此时为time_delta
        if (!prev_chunk) {//type1 必须有前面的chunk作为基础
            spdlog::error("no prev chunk, csid:{}", csid);
            co_return -8;
        }
        chunk->chunk_message_header_.message_stream_id_ = prev_chunk->chunk_message_header_.message_stream_id_;
        // *chunk = *prev_chunk;
        if (left_size < 3) {
            co_return 0;
        }

        int32_t time_delta = 0;
        uint8_t *p = (uint8_t *)&time_delta;
        p[0] = recv_buffer_[buf_pos + 2];
        p[1] = recv_buffer_[buf_pos + 1];
        p[2] = recv_buffer_[buf_pos + 0];
        buf_pos += 3;
        left_size -= 3;

        if (time_delta != 0xffffff) {
            chunk->chunk_message_header_.timestamp_ = prev_chunk->chunk_message_header_.timestamp_ + time_delta;
            chunk->chunk_message_header_.timestamp_delta_ = time_delta;
        }
        
        if (left_size < 3) {
            co_return 0;
        }

        p = (uint8_t *)(&(chunk->chunk_message_header_.message_length_));
        p[0] = recv_buffer_[buf_pos + 2];
        p[1] = recv_buffer_[buf_pos + 1];
        p[2] = recv_buffer_[buf_pos + 0];
        buf_pos += 3;
        left_size -= 3;

        if (left_size < 1) {
            co_return 0;
        }
        chunk->chunk_message_header_.message_type_id_ = recv_buffer_[buf_pos];
        buf_pos++;
        left_size--;

        if (time_delta == 0xffffff) {
            if (left_size < 4) {
                co_return 0;
            }

            p = (uint8_t*)&time_delta;
            p[0] = recv_buffer_[buf_pos + 3];
            p[1] = recv_buffer_[buf_pos + 2];
            p[2] = recv_buffer_[buf_pos + 1];
            p[3] = recv_buffer_[buf_pos + 0];
            buf_pos += 4;
            left_size -= 4;
            chunk->chunk_message_header_.timestamp_ = prev_chunk->chunk_message_header_.timestamp_ + time_delta;
            chunk->chunk_message_header_.timestamp_delta_ = time_delta;
        }
    } else if (fmt == RTMP_CHUNK_FMT_TYPE2) {//此时为time_delta
        if (!prev_chunk) {//type2 必须有前面的chunk作为基础
            co_return -12;
        }

        chunk->chunk_message_header_.message_length_ = prev_chunk->chunk_message_header_.message_length_;
        chunk->chunk_message_header_.message_type_id_ = prev_chunk->chunk_message_header_.message_type_id_;
        chunk->chunk_message_header_.message_stream_id_ = prev_chunk->chunk_message_header_.message_stream_id_;
        if (left_size < 3) {
            co_return 0;
        }
        int32_t time_delta = 0;
        uint8_t *p = (uint8_t *)&time_delta;
        p[0] = recv_buffer_[buf_pos + 2];
        p[1] = recv_buffer_[buf_pos + 1];
        p[2] = recv_buffer_[buf_pos + 0];
        buf_pos += 3;
        left_size -= 3;
        if (time_delta != 0xffffff) {
            chunk->chunk_message_header_.timestamp_ = prev_chunk->chunk_message_header_.timestamp_ + time_delta;
            chunk->chunk_message_header_.timestamp_delta_ = time_delta;
        } else {
            if (left_size < 4) {
                co_return 0;
            }

            p = (uint8_t*)&time_delta;
            p[0] = recv_buffer_[buf_pos + 3];
            p[1] = recv_buffer_[buf_pos + 2];
            p[2] = recv_buffer_[buf_pos + 1];
            p[3] = recv_buffer_[buf_pos + 0];
            buf_pos += 4;
            left_size -= 4;
            chunk->chunk_message_header_.timestamp_ = prev_chunk->chunk_message_header_.timestamp_ + time_delta;
            chunk->chunk_message_header_.timestamp_delta_ = time_delta;
        }
    } else if (fmt == RTMP_CHUNK_FMT_TYPE3) {
        if (!prev_chunk) {
            co_return -13;
        }

        chunk->chunk_message_header_.message_length_ = prev_chunk->chunk_message_header_.message_length_;
        chunk->chunk_message_header_.message_type_id_ = prev_chunk->chunk_message_header_.message_type_id_;
        chunk->chunk_message_header_.message_stream_id_ = prev_chunk->chunk_message_header_.message_stream_id_;
        chunk->chunk_message_header_.timestamp_ = prev_chunk->chunk_message_header_.timestamp_;
        chunk->chunk_message_header_.timestamp_delta_ = prev_chunk->chunk_message_header_.timestamp_delta_;
    }

    // 前面有一个chunk，并且前面的chunk只要rtmp_message_变量非nullptr，则必然没有接收完整
    if (prev_chunk && prev_chunk->rtmp_message_) {
        chunk->rtmp_message_ = prev_chunk->rtmp_message_;
        // chunk回收，不用每次解析够创建一个
        prev_chunk->clear();
        recv_chunk_cache_[csid] = prev_chunk;
    }
    // 重新记录前一个chunk
    recv_chunk_streams_[csid] = chunk;
    // 安全规则
    if (chunk->chunk_message_header_.message_length_ >= 2*1024*1024) {// packet too big
        co_return -14;
    }
    // 之前没有rtmp message记录，那么就需要创建一个
    if (!chunk->rtmp_message_) {
        chunk->rtmp_message_ = std::make_shared<RtmpMessage>(chunk->chunk_message_header_.message_length_);
    }
    // 处理chunk的payload
    int32_t this_chunk_payload_size = std::min(in_chunk_size_, chunk->chunk_message_header_.message_length_ - (int32_t)chunk->rtmp_message_->get_using_data().size());
    if (left_size < this_chunk_payload_size) {
        co_return 0;
    }

    memcpy((void*)chunk->rtmp_message_->get_unuse_data().data(), recv_buffer_ + buf_pos, this_chunk_payload_size);
    chunk->rtmp_message_->inc_used_bytes(this_chunk_payload_size);
    buf_pos += this_chunk_payload_size;
    left_size -= this_chunk_payload_size;

    // if we get a rtmp message
    if ((int32_t)chunk->rtmp_message_->get_using_data().size() == chunk->chunk_message_header_.message_length_) {
        // todo fill rtmp message type...
        chunk->rtmp_message_->chunk_stream_id_ = csid;
        chunk->rtmp_message_->timestamp_ = chunk->chunk_message_header_.timestamp_;
        chunk->rtmp_message_->message_type_id_ = chunk->chunk_message_header_.message_type_id_;
        chunk->rtmp_message_->message_stream_id_ = chunk->chunk_message_header_.message_stream_id_;

        if (!recv_handler_) {
            chunk->rtmp_message_ = nullptr;
            spdlog::error("no recv handler set for rtmp msg");
            co_return -19;
        }

        bool ret = co_await recv_handler_(chunk->rtmp_message_);
        if (!ret) {
            spdlog::error("process rtmp message failed, close connection, type:{}", (int32_t)chunk->rtmp_message_->message_type_id_);
            co_return -20;
        }

        chunk->rtmp_message_ = nullptr;
    } else if (chunk->rtmp_message_->get_total_data().size() >= 2*1024*1024) {//安全规则
        spdlog::error("rtmp max bytes error");
        co_return -21;
    }
    co_return buf_pos;
}

bool RtmpChunkProtocol::is_protocol_control_message(std::shared_ptr<RtmpMessage> msg) {
    if (msg->chunk_stream_id_ != RTMP_CHUNK_ID_PROTOCOL_CONTROL_MESSAGE) {
        return false;
    }

    if (msg->message_stream_id_ != 0) {
        return false;
    }

    if (msg->message_type_id_ != RTMP_CHUNK_MESSAGE_TYPE_SET_CHUNK_SIZE &&
        msg->message_type_id_ != RTMP_CHUNK_MESSAGE_TYPE_ABORT_MESSAGE &&
        msg->message_type_id_ != RTMP_CHUNK_MESSAGE_TYPE_WINDOW_ACK_SIZE &&
        msg->message_type_id_ != RTMP_CHUNK_MESSAGE_TYPE_ACKNOWLEDGEMENT && 
        msg->message_type_id_ != RTMP_CHUNK_MESSAGE_TYPE_SET_PEER_BANDWIDTH) {
        return false;
    }
    return true;
}

boost::asio::awaitable<bool> RtmpChunkProtocol::handle_protocol_control_message(std::shared_ptr<RtmpMessage> msg) {
    switch(msg->message_type_id_) {
        case RTMP_CHUNK_MESSAGE_TYPE_SET_CHUNK_SIZE: {
            if (!handle_set_chunk_size(msg)) {
                co_return false;
            } 
            co_return true;
        }
        case RTMP_CHUNK_MESSAGE_TYPE_ABORT_MESSAGE: {
            if (!handle_abort(msg)) {
                co_return false;
            }
            co_return true;
        }
        case RTMP_CHUNK_MESSAGE_TYPE_WINDOW_ACK_SIZE: {
            if (!handle_window_acknowledge_size(msg)) {
                co_return false;
            }
            co_return true;
        }
    }
    co_return true;
}

bool RtmpChunkProtocol::handle_set_chunk_size(std::shared_ptr<RtmpMessage> msg) {
    RtmpSetChunkSizeMessage cmd;
    int ret = cmd.decode(msg);
    if (ret <= 0) {// 解析成功时，返回解析成功字节数
        spdlog::error("decode set chunk packet failed");
        return false;
    }
    in_chunk_size_ = cmd.chunk_size_;
    spdlog::debug("set in chunk size:{}", in_chunk_size_);
    return true;
}


bool RtmpChunkProtocol::handle_abort(std::shared_ptr<RtmpMessage> msg) {
    RtmpAbortMessage cmd;
    int ret = cmd.decode(msg);
    if (ret <= 0) {// 解析成功时，返回解析成功字节数
        return false;
    }
    auto it = recv_chunk_streams_.find(cmd.chunk_id_);
    if (it != recv_chunk_streams_.end()) {
        recv_chunk_streams_.erase(it);
    }
    return true;
}

bool RtmpChunkProtocol::handle_window_acknowledge_size(std::shared_ptr<RtmpMessage> msg) {
    RtmpWindowAcknwledgeSizeMessage cmd;
    int ret = cmd.decode(msg);
    if (ret <= 0) {// 解析成功时，返回解析成功字节数
        spdlog::error("decode window acknowledge size msg failed");
        return false;
    }
    in_window_acknowledge_size_ = cmd.acknowledge_windows_size_;
    return true;
}
