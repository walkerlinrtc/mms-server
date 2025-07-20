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
#include <string.h>
#include <vector>
#include <memory>

#include "base/packet.h"

namespace mms {
#define RTMP_CHUNK_FMT_TYPE0 0
#define RTMP_CHUNK_FMT_TYPE1 1
#define RTMP_CHUNK_FMT_TYPE2 2
#define RTMP_CHUNK_FMT_TYPE3 3
// rtmp chunk id spec(csid 2固定用作protocol control message)
// @see rtmp_specification_1.0.pdf
// 6.1.1. Chunk Basic Header 
// The protocol supports up to 65597 streams with IDs 3–65599.
// The IDs 0, 1, and 2 are reserved. Value 0 indicates the ID in the range of 64–319 (the second byte + 64). 
// Value 1 indicates the ID in the range of 64–65599 ((the third byte)*256 + the second byte + 64). 
// Value 2 indicates its low-level protocol message
// 
// 5. Protocol Control Messages
// Protocol control messages MUST have message stream ID 0 (called as control stream) and chunk stream ID 2, 
// and are sent with highest priority
#define RTMP_CHUNK_ID_PROTOCOL_CONTROL_MESSAGE      2
#define RTMP_CHUNK_ID_COMMAND_MESSAGE               3

// rtmp message type spec
// @see rtmp_sepcification_1.0.pdf
// 3.1. Command message
// Command messages carry the AMF-encoded commands between the client and the server. 
// These messages have been assigned message type value of 20 for AMF0 encoding and message type value of 17 for AMF3 encoding.
#define RTMP_MESSAGE_TYPE_AMF0_COMMAND              20
#define RTMP_MESSAGE_TYPE_AMF3_COMMAND              17
// 3.2. Data message 
// The client or the server sends this message to send Metadata or any user data to the peer. 
// Metadata includes details about the data(audio, video etc.) like creation time, duration, theme and so on. 
// These messages have been assigned message type value of 18 for AMF0 and message type value of 15 for AMF3.
#define RTMP_MESSAGE_TYPE_AMF0_DATA                 18
#define RTMP_MESSAGE_TYPE_AMF3_DATA                 15
// 3.3. Shared object message 
// A shared object is a Flash object (a collection of name value pairs) that are in synchronization across multiple clients, 
// instances, and so on. 
// The message types kMsgContainer=19 for AMF0 and kMsgContainerEx=16 for AMF3 are reserved for shared object events. 
// Each message can contain multiple events.
// 上面的说明中kMsgContainer=19 这个kMsgContainer不知道突然从哪里冒出来的，其实就是指message type等于19.
#define RTMP_MESSAGE_TYPE_AMF0_SHARED_OBJ           19
#define RTMP_MESSAGE_TYPE_AMF3_SHARED_OBJ           16
// 3.4. Audio message
// The client or the server sends this message to send audio data to the peer. 
// The message type value of 8 is reserved for audio messages. 
#define RTMP_MESSAGE_TYPE_AUDIO                     8
// 3.5. Video message
// The client or the server sends this message to send video data to the peer. 
// The message type value of 9 is reserved for video messages. 
// These messages are large and can delay the sending of other type of messages. 
// To avoid such a situation, the video message is assigned the lowest priority. 
#define RTMP_MESSAGE_TYPE_VIDEO                     9
// 3.6. Aggregate message 
// An aggregate message is a single message that contains a list of submessages. 
// The message type value of 22 is reserved for aggregate messages. 
#define RTMP_MESSAGE_TYPE_AGGREGATE                 22
// 下面是prococol message 的message type定义
// 5. Protocol Control Messages
#define RTMP_CHUNK_MESSAGE_TYPE_SET_CHUNK_SIZE            1
#define RTMP_CHUNK_MESSAGE_TYPE_ABORT_MESSAGE             2
#define RTMP_CHUNK_MESSAGE_TYPE_ACKNOWLEDGEMENT           3
#define RTMP_MESSAGE_TYPE_USER_CONTROL                    4
#define RTMP_CHUNK_MESSAGE_TYPE_WINDOW_ACK_SIZE           5
#define RTMP_CHUNK_MESSAGE_TYPE_SET_PEER_BANDWIDTH        6
// rtmp message id spec(rtmp 的protocol control message固定message id为0)
#define RTMP_MESSAGE_ID_PROTOCOL_CONTROL            0
// rtmp user event类型定义
// 3.7. User Control message
// The client or the server sends this message to notify the peer about the user control events. 
// For information about the message format, refer to the User Control Messages section in the RTMP Message Foramts draft. 
// The following user control event types are supported: 
#define RTMP_USER_EVENT_STREAM_BEGIN                0x0000
#define RTMP_USER_EVENT_STREAM_EOF                  0x0001
#define RTMP_USER_EVENT_STREAM_DRY                  0x0002
#define RTMP_USER_EVENT_SET_BUFFER_LENGTH           0x0003
#define RTMP_USER_EVENT_STREAM_IS_RECORDED          0x0004
#define RTMP_USER_EVENT_PING_REQUEST                0x0006
#define RTMP_USER_EVENT_PING_RESPONSE               0x0007

// rtmp command定义
// 4. Types of commands
// The client and the server exchange commands which are AMF encoded. The sender sends a command message
// that consists of command name, transaction ID, and command object that contains related parameters. 
// For example, the connect command contains ‘app’ parameter, which tells the server application name the 
// client is connected to. The receiver processes the command and sends back the response with the same 
// transaction ID. The response string is either _result, _error, or a method name, for example, verifyClient 
// or contactExternalServer. A command string of _result or _error signals a response. The transaction ID 
// indicates the outstanding command to which the response refers. It is identical to the tag in IMAP and many 
// other protocols. The method name in the command string indicates that the sender is trying to run a method 
// on the receiver end. 
// The following class objects are used to send various commands: 
// o NetConnection – An object that is a higher-level representation of connection between the server and the client. 
// o NetStream – An object that represents the channel over which audio streams, video streams and other related data are sent.
// We also send commands like play , pause etc. which control the flow of the data.
// 有两种类型的command，一种是和连接相关的，称为NetConnection类型;另一种是和流相关的，称为NetStream类型
// 连接相关的有：connect、call、close、createStream（可以理解为流创建前后的命令）
// NetConnection的命令，如果需要响应，通过_result command回复
// NetStream的命令，如果需要响应，通过onStatus command回复
// 4.1.1. connect
// The client sends the connect command to the server to request connection to a server application instance. 
#define RTMP_COMMAND_NAME_CONNECT                   "connect"
// 4.1.2. Call (没有具体的command name，command name自定义)
// The call method of the NetConnection object runs remote procedure calls (RPC) at the receiving end. 
// The called RPC name is passed as a parameter to the call command. 
// 4.1.3. createStream
// The client sends this command to the server to create a logical channel for message communication The publishing of audio, 
// video, and metadata is carried out over stream channel created using the createStream command. NetConnection is the default 
// communication channel, which has a stream ID 0. Protocol and a few command messages, including createStream, use the default 
// communication channel
#define RTMP_COMMAND_NAME_CREATESTREAM              "createStream"
// 文档上面明明写了4个NetConnection命令，可是最后却不说close命令 

// 4.2. NetStream commands
// The NetStream defines the channel through which the streaming audio, video, and data messages can flow over the NetConnection 
// that connects the client to the server. A NetConnection object can support multiple NetStreams for multiple data streams.
// 就是说，NetConnection object代表的一个连接，可以传输多个NetStream对象（不过目前直播都只有一个NetStream）
// The following commands can be sent on the NetStream : 
// o play 
// o play2 
// o deleteStream 
// o closeStream 
// o receiveAudio 
// o receiveVideo 
// o publish 
// o seek 
// o pause
// 4.2.1. play
// The client sends this command to the server to play a stream.
#define RTMP_COMMAND_NAME_PLAY                      "play"  //包含一个Stream Name字符串：Name of the stream to play.
// 4.2.2. play2
// Unlike the play command, play2 can switch to a different bit rate stream without changing the timeline of the content played.
#define RTMP_COMMAND_NAME_PLAY2                     "play2" // 这个一般不支持
// 4.2.3. deleteStream 
// NetStream sends the deleteStream command when the NetStream object is getting destroyed.
#define RTMP_COMMAND_NAME_DELETESTREAM              "deleteStream"
// 4.2.4. receiveAudio
// NetStream sends the receiveAudio message to inform the server whether to send or not to send the audio to the client. 
#define RTMP_COMMAND_NAME_RECEIVEAUDIO              "receiveAudio"
// 4.2.5. receiveVideo 
// NetStream sends the receiveVideo message to inform the server whether to send the video to the client or not.
#define RTMP_COMMAND_NAME_RECEIVEVIDEO              "receiveVideo"
// 4.2.6. Publish
// The client sends the publish command to publish a named stream to the server. Using this name, 
// any client can play this stream and receive the published audio, video, and data messages. 
#define RTMP_COMMAND_NAME_PUBLISH                   "publish"
// 4.2.7. seek
// The client sends the seek command to seek the offset (in milliseconds) within a media file or playlist.
#define RTMP_COMMAND_NAME_SEEK                      "seek"
// 4.2.8. pause 
// The client sends the pause command to tell the server to pause or start playing.
#define RTMP_COMMAND_NAME_PAUSE                     "pause"

#define RTMP_COMMAND_NAME_RESULT                    "_result"
#define RTMP_COMMAND_NAME_ERROR                     "_error"

// code value
#define RTMP_RESULT_CONNECT_SUCCESS                 "NetConnection.Connect.Success"
#define RTMP_RESULT_CONNECT_REJECTED                "NetConnection.Connect.Rejected"
#define RTMP_STATUS_STREAM_PLAY_RESET               "NetStream.Play.Reset"
#define RTMP_STATUS_STREAM_PLAY_START               "NetStream.Play.Start"
#define RTMP_STATUS_STREAM_PLAY_FAILED              "NetStream.Play.Failed"
#define RTMP_STATUS_STREAM_PAUSE_NOTIFY             "NetStream.Pause.Notify"
#define RTMP_STATUS_STREAM_UNPAUSE_NOTIFY           "NetStream.Unpause.Notify"
#define RTMP_STATUS_STREAM_PUBLISH_START            "NetStream.Publish.Start"
#define RTMP_STATUS_STREAM_DATA_START               "NetStream.Data.Start"
#define RTMP_STATUS_STREAM_UNPUBLISH_SUCCESS        "NetStream.Unpublish.Success"
#define RTMP_STATUS_STREAM_NOT_FOUND                "NetStream.Play.StreamNotFound"

class RtmpChunk;

class ChunkMessageHeader { 
public:
    int8_t  fmt_ = 0;
    int32_t timestamp_delta_ = 0;
    int32_t timestamp_ = 0;
    int32_t message_length_ = 0;
    uint8_t message_type_id_ = 0;
    int32_t message_stream_id_ = 0;
    void clear() {
        fmt_ = 0;
        timestamp_delta_ = 0;
        timestamp_ = 0;
        message_length_ = 0;
        message_type_id_ = 0;
        message_stream_id_ = 0;
    }
};

class RtmpMessage : public Packet {
public:
    // todo payload最大长度和实际使用长度分开
    RtmpMessage(int32_t payload_size);
    virtual ~RtmpMessage();
    uint8_t get_message_type();
public:
    uint8_t message_type_id_;
    int32_t timestamp_;
    int32_t message_stream_id_;
    int32_t chunk_stream_id_ = 0;
};

class RtmpChunk {
public:
    // RtmpChunk& operator=(RtmpChunk & c) {
    //     memcpy(&this->chunk_message_header_, &c.chunk_message_header_, sizeof(ChunkMessageHeader));
    //     this->rtmp_message_ = c.rtmp_message_;
    //     c.rtmp_message_ = nullptr;
    //     return *this;
    // }

    void clear() {
        chunk_message_header_.clear();
        if (rtmp_message_) {
            rtmp_message_ = nullptr;
        }
    }
public:
    ChunkMessageHeader chunk_message_header_;
    std::shared_ptr<RtmpMessage> rtmp_message_;
};

};