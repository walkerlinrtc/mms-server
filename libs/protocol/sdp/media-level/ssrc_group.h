#pragma once
#include <stdint.h>
#include <string>
#include <unordered_map>
#include "protocol/sdp/webrtc/ssrc.h"
// @refer https://tools.ietf.org/html/rfc5576#section-4
// 2.10.1 ssrc-group#
// 定义一组相关联的 ssrc：

// a=ssrc-group:<semantics> <ssrc-id> ...
// semantics：可以取值为 FID(Flow Identification)、FEC(Forward Error Correction)
// ssrc-id：可以有多个，表明是一组相关联的 ssrc
// 在 rtx 的语义中，rtx 重传包不仅有不同于被重传包的 pt 值，也有不同的 ssrc 值，rtx ssrc 在 ssrc-group 字段中，如下是 video media level 示例：
// a=ssrc-group:FID 3473705511 683075964
// a=ssrc:3473705511 cname:wJobAtYmVV7u5Y3x
// a=ssrc:3473705511 msid:g4n21yfXiVM9K1CrtoSZMNxoNI7kBbY9rHgp 0befb6f8-46a8-4890-9955-b53689962daf
// a=ssrc:3473705511 mslabel:g4n21yfXiVM9K1CrtoSZMNxoNI7kBbY9rHgp
// a=ssrc:3473705511 label:0befb6f8-46a8-4890-9955-b53689962daf
// a=ssrc:683075964 cname:wJobAtYmVV7u5Y3x
// a=ssrc:683075964 msid:g4n21yfXiVM9K1CrtoSZMNxoNI7kBbY9rHgp 0befb6f8-46a8-4890-9955-b53689962daf
// a=ssrc:683075964 mslabel:g4n21yfXiVM9K1CrtoSZMNxoNI7kBbY9rHgp
// a=ssrc:683075964 label:0befb6f8-46a8-4890-9955-b53689962daf
// 其中，semantics 为 FID，ssrc-id 有两个，第一个是正常 media level 的 ssrc，第二个 ssrc 即 rtx 的重传 ssrc。
namespace mms {

struct SsrcGroup {
public:
    static std::string prefix;
    bool parse(const std::string & line);
    bool parse_ssrc(const std::string & line);
    std::string to_string() const;
    std::unordered_map<uint32_t, Ssrc> & get_ssrcs() {
        return ssrcs_;
    }

    void add_ssrc(const Ssrc & ssrc) {
        ssrcs_[ssrc.get_id()] = ssrc;
    }
public:
    std::string semantics;
    std::unordered_map<uint32_t, Ssrc> ssrcs_;
};

};