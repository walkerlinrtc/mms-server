#pragma once
// @draft-ietf-rtcweb-sdp-14.html
// rfc8285 中同一个 RTPStream 中允许 one-byte header extension 和 two-byte header extension 同时出现，需要 sdp中声明 ‘a=extmap-allow-mixed’
// 在 SDP 信息中 ‘a=extmap’ 用来描述 RTP header extension。
// a=extmap:<value>["/"<direction>] <URI> <extensionattributes>
// a=extmap:1 http://example.com/082005/ext.htm#ttime
// a=extmap:2/sendrecv http://example.com/082005/ext.htm#xmeta
// 参考这个文章：https://blog.csdn.net/aggresss/article/details/106436703

// https://blog.jianchihu.net/webrtc-research-rtp-header-extension.html
// RTP Header Extension
// 如果RTP Fixed Header中，X字段为1，说明后面跟着RTP Header Extension。RTP Header Extension结构如下：

//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |      defined by profile       |           length              |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                        header extension                       |
//    |                             ....                              |
// defined by profile：决定使用哪种Header Extension：one-byte或者two-byte header
// length:表示Header Extension的长度：length x 4字节
// One-Byte Header
// 对于One-Byte Header，"defined by profile"字段为固定的0xBEDE。接着后面的结构如下：
// Two-Byte Header
// "defined by profile"字段结构如下：

//        0                   1
//        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |         0x100         |appbits|
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 接着后面跟着的每个扩展元素结构如下：

//        0                   1
//        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |       ID      |     length    |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// ID：本地标识符
// length:表示extension data长度，范围1~255
// 如下是一个Two-Byte Header示例：

//        0                   1                   2                   3
//        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |       0x10    |    0x00       |           length=3            |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |      ID       |     L=0       |     ID        |     L=1       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |       data    |    0 (pad)    |       ID      |      L=4      |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |                          data                                 |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 首先"defined by profile"字段为0x1000，length为3，后面跟着3x4字节长度扩展，对于第一个header extension：L=0，数据长度为0，对于第二个header extension：L=1，data长度为1，接着是填充数据，对于第三个header extension：L=4，后面跟着4字节长度数据。

// 由于WebRTC中默认都是One-Byte Header，所以就不抓包分析了，具体构造解析代码跟One-Byte Header位于同一地方。

// 常见RTP Header Extension
#include <string>
#include <vector>

namespace mms {
struct Extmap {
public:
    static std::string prefix;
    virtual bool parse(const std::string & line);
public:
    std::string value;
    std::string direction;
    std::string uri;
    std::vector<std::string> ext_attrs;
};
};