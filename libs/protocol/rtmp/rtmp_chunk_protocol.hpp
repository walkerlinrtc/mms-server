/*
MIT License

Copyright (c) 2021 jiangbaolin

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once

#include <functional>
#include <unordered_map>
#include <memory>
#include <vector>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>

#include "rtmp_define.hpp"
#include "protocol/rtmp/rtmp_message/chunk_message/rtmp_set_chunk_size_message.hpp"
#include "protocol/rtmp/rtmp_message/chunk_message/rtmp_abort_message.hpp"

namespace mms {
class SocketInterface;
class RtmpMessage;

constexpr int64_t MAX_RTMP_BUF_BYTES = 2*1024*1024;
constexpr int64_t MIN_RTMP_BUF_BYTES = 16*1024;

class RtmpChunkProtocol {
public:
    RtmpChunkProtocol(std::shared_ptr<SocketInterface> conn);
    virtual ~RtmpChunkProtocol();
public:
    // 循环接收消息
    boost::asio::awaitable<int32_t> cycle_recv_rtmp_message(const std::function<boost::asio::awaitable<bool>(std::shared_ptr<RtmpMessage>)> & recv_handler);
    inline size_t get_out_chunk_size() {
        return out_chunk_size_;
    }

    inline void set_out_chunk_size(size_t s) {
        out_chunk_size_ = s;
    }

    inline void set_in_chunk_size(int32_t s) {
        in_chunk_size_ = s;
    }

    inline int32_t get_in_chunk_size() {
        return in_chunk_size_;
    }

    // 异步发送多个 rtmp 消息
    boost::asio::awaitable<bool> send_rtmp_messages(const std::vector<std::shared_ptr<RtmpMessage>> & rtmp_msgs);

    int64_t get_last_ack_bytes() {
        return last_ack_bytes_;
    }

    inline void set_last_ack_bytes(int64_t v) {
        last_ack_bytes_ = v;
    }

    int64_t get_in_window_acknowledge_size() {
        return in_window_acknowledge_size_;
    }

    inline void set_in_window_acknowledge_size(int64_t v) {
        in_window_acknowledge_size_ = v;
    }

    // protocol control 消息处理
    bool is_protocol_control_message(std::shared_ptr<RtmpMessage> msg);
    boost::asio::awaitable<bool> handle_protocol_control_message(std::shared_ptr<RtmpMessage> msg);
private:
    boost::asio::awaitable<int32_t> process_recv_buffer();
    
    bool handle_set_chunk_size(std::shared_ptr<RtmpMessage> msg);
    bool handle_abort(std::shared_ptr<RtmpMessage> msg);
    bool handle_window_acknowledge_size(std::shared_ptr<RtmpMessage> msg);
private:
    // 底层Socket连接
    std::shared_ptr<SocketInterface> conn_;
    std::function<boost::asio::awaitable<bool>(std::shared_ptr<RtmpMessage>)> recv_handler_ = {};

    // 接收缓冲
    uint8_t *recv_buffer_ = nullptr;
    int64_t max_buf_bytes_ = MIN_RTMP_BUF_BYTES;
    int64_t recv_len_ = 0;

    int32_t in_chunk_size_ = 128;
    int32_t out_chunk_size_ = 128;

    int64_t in_window_acknowledge_size_ = 5*1024*1024;
    int64_t last_ack_bytes_ = 0;
    
    // 上次发送的chunk流，用于下次判断chunk fmt
    std::unordered_map<uint32_t, std::shared_ptr<RtmpChunk>> send_chunk_streams_;
    std::vector<std::unique_ptr<char[]>> chunk_headers_;
    // 发送缓冲
    std::vector<boost::asio::const_buffer> send_sv_bufs_;

    std::unordered_map<uint32_t, std::shared_ptr<RtmpChunk>> recv_chunk_streams_;
    std::unordered_map<uint32_t, std::shared_ptr<RtmpChunk>> recv_chunk_cache_;
};
};
