#pragma once
#include <string>
namespace mms {
// 5.7.  Connection Data ("c=")
//    c=<nettype> <addrtype> <connection-address>
//    The "c=" field contains connection data.

//    A session description MUST contain either at least one "c=" field in
//    each media description or a single "c=" field at the session level.
//    It MAY contain a single session-level "c=" field and additional "c="
//    field(s) per media description, in which case the per-media values
//    override the session-level settings for the respective media.

//    The first sub-field ("<nettype>") is the network type, which is a
//    text string giving the type of network.  Initially, "IN" is defined
//    to have the meaning "Internet", but other values MAY be registered in
//    the future (see Section 8).

//    The second sub-field ("<addrtype>") is the address type.  This allows
//    SDP to be used for sessions that are not IP based.  This memo only
//    defines IP4 and IP6, but other values MAY be registered in the future
//    (see Section 8).

//    The third sub-field ("<connection-address>") is the connection
//    address.  OPTIONAL sub-fields MAY be added after the connection
//    address depending on the value of the <addrtype> field.

//    When the <addrtype> is IP4 and IP6, the connection address is defined
//    as follows:

//    o  If the session is multicast, the connection address will be an IP
//       multicast group address.  If the session is not multicast, then
//       the connection address contains the unicast IP address of the
//       expected data source or data relay or data sink as determined by
//       additional attribute fields.  It is not expected that unicast
//       addresses will be given in a session description that is
//       communicated by a multicast announcement, though this is not
//       prohibited.

//    o  Sessions using an IPv4 multicast connection address MUST also have
//       a time to live (TTL) value present in addition to the multicast
//       address.  The TTL and the address together define the scope with
//       which multicast packets sent in this conference will be sent.  TTL
//       values MUST be in the range 0-255.  Although the TTL MUST be
//       specified, its use to scope multicast traffic is deprecated;
//       applications SHOULD use an administratively scoped address
//       instead.

//    The TTL for the session is appended to the address using a slash as a
//    separator.  An example is:

//       c=IN IP4 224.2.36.42/127

//    IPv6 multicast does not use TTL scoping, and hence the TTL value MUST
//    NOT be present for IPv6 multicast.  It is expected that IPv6 scoped
//    addresses will be used to limit the scope of conferences.

//    Hierarchical or layered encoding schemes are data streams where the
//    encoding from a single media source is split into a number of layers.
//    The receiver can choose the desired quality (and hence bandwidth) by
//    only subscribing to a subset of these layers.  Such layered encodings
//    are normally transmitted in multiple multicast groups to allow
//    multicast pruning.  This technique keeps unwanted traffic from sites
//    only requiring certain levels of the hierarchy.  For applications
//    requiring multiple multicast groups, we allow the following notation
//    to be used for the connection address:

//       <base multicast address>[/<ttl>]/<number of addresses>

//    If the number of addresses is not given, it is assumed to be one.
//    Multicast addresses so assigned are contiguously allocated above the
//    base address, so that, for example:

//       c=IN IP4 224.2.1.1/127/3

//    would state that addresses 224.2.1.1, 224.2.1.2, and 224.2.1.3 are to
//    be used at a TTL of 127.  This is semantically identical to including
//    multiple "c=" lines in a media description:

//       c=IN IP4 224.2.1.1/127
//       c=IN IP4 224.2.1.2/127
//       c=IN IP4 224.2.1.3/127

//    Similarly, an IPv6 example would be:

//       c=IN IP6 FF15::101/3

//    which is semantically equivalent to:

//       c=IN IP6 FF15::101
//       c=IN IP6 FF15::102
//       c=IN IP6 FF15::103
struct ConnectionInfo {
public:
    static std::string prefix;
    ConnectionInfo() = default;
    ConnectionInfo(const std::string &nettype, const std::string & addrtype, const std::string & conn_addr) : nettype_(nettype), addrtype_(addrtype), connection_address_(conn_addr) {

    }
    
    bool parse(const std::string & line);

    const std::string & get_net_type() {
        return nettype_;
    }

    void set_net_type(const std::string & nt) {
        nettype_ = nt;
    }

    const std::string & get_addr_type() {
        return addrtype_;
    }

    void set_addr_type(const std::string & at) {
        addrtype_ = at;
    }

    const std::string & get_connection_address() {
        return connection_address_;
    }

    void set_connection_address(const std::string & ca) {
        connection_address_ = ca;
    }

    int32_t get_ttl() {
        return ttl;
    }

    void set_ttl(int32_t t) {
        ttl = t;
    }

    int32_t get_num_of_addr() {
        return num_of_addr;
    }

    void set_num_of_addr(int32_t val) {
        num_of_addr = val;
    }

    std::string to_string() const;
public:
    //    The first sub-field ("<nettype>") is the network type, which is a
    //    text string giving the type of network.  Initially, "IN" is defined
    //    to have the meaning "Internet", but other values MAY be registered in
    //    the future (see Section 8).
    std::string nettype_;
    //    The second sub-field ("<addrtype>") is the address type.  This allows
    //    SDP to be used for sessions that are not IP based.  This memo only
    //    defines IP4 and IP6, but other values MAY be registered in the future
    //    (see Section 8).
    std::string addrtype_;
    //    The third sub-field ("<connection-address>") is the connection
    //    address.  OPTIONAL sub-fields MAY be added after the connection
    //    address depending on the value of the <addrtype> field.
    // 目前只支持单播
    std::string connection_address_;
    int32_t ttl = 0;
    int32_t num_of_addr = 1;
};
};