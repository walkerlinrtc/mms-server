#pragma once
#include <string>
#include <cstdint>
namespace mms {
// 2.1.  The RTCP Attribute

//    The RTCP attribute is used to document the RTCP port used for media
//    stream, when that port is not the next higher (odd) port number
//    following the RTP port described in the media line.  The RTCP
//    attribute is a "value" attribute, and follows the general syntax
//    specified page 18 of [RFC2327]: "a=<attribute>:<value>".  For the
//    RTCP attribute:

//    *  the name is the ascii string "rtcp" (lower case),

//    *  the value is the RTCP port number and optional address.

//    The formal description of the attribute is defined by the following
//    ABNF [RFC2234] syntax:

//    rtcp-attribute =  "a=rtcp:" port  [nettype space addrtype space
//                          connection-address] CRLF

struct RtcpAttr {
public:
    static std::string prefix;
    bool parse(const std::string & line);

    uint16_t get_port() const {
        return port;
    }

    void set_port(uint16_t val) {
        port = val;
    }
    
    const std::string & get_net_type() {
        return nettype;
    }

    void set_net_type(const std::string & nt) {
        nettype = nt;
    }

    const std::string & get_addr_type() {
        return addrtype;
    }

    void set_addr_type(const std::string & at) {
        addrtype = at;
    }

    const std::string & get_connection_address() {
        return connection_address;
    }

    void set_connection_address(const std::string & ca) {
        connection_address = ca;
    }

    std::string to_string() const;
public:
    uint16_t port;
    //    The first sub-field ("<nettype>") is the network type, which is a
    //    text string giving the type of network.  Initially, "IN" is defined
    //    to have the meaning "Internet", but other values MAY be registered in
    //    the future (see Section 8).
    std::string nettype;
    //    The second sub-field ("<addrtype>") is the address type.  This allows
    //    SDP to be used for sessions that are not IP based.  This memo only
    //    defines IP4 and IP6, but other values MAY be registered in the future
    //    (see Section 8).
    std::string addrtype;
    //    The third sub-field ("<connection-address>") is the connection
    //    address.  OPTIONAL sub-fields MAY be added after the connection
    //    address depending on the value of the <addrtype> field.
    // 目前只支持单播
    std::string connection_address;
};
};