#pragma once
#include <string>
// 5.2.  Origin ("o=")
namespace mms {
struct Origin {
public:
    static std::string prefix;
    Origin() = default;
    Origin(const std::string & username, uint64_t session_id, uint32_t session_version, const std::string & nettype, const std::string & addrtype, const std::string & unicast_address) {
        username_ = username;
        session_id_ = session_id;
        session_version_ = session_version;
        nettype_ = nettype;
        addrtype_ = addrtype;
        unicast_address_ = unicast_address;
    }
    bool parse(const std::string & line);

    void set(const std::string & username, uint64_t session_id, uint32_t session_version, const std::string & nettype, const std::string & addrtype, const std::string & unicast_address) {
        username_ = username;
        session_id_ = session_id;
        session_version_ = session_version;
        nettype_ = nettype;
        addrtype_ = addrtype;
        unicast_address_ = unicast_address;
    }

    inline const std::string & get_user_name() const {
        return username_;
    }

    inline void set_user_name(const std::string & u) {
        username_ = u;
    }

    inline uint64_t get_session_id() {
        return session_id_;
    }

    void set_session_id(uint64_t sid) {
        session_id_ = sid;
    }

    uint32_t get_session_version() {
        return session_version_;
    }

    void set_session_version(uint32_t v) {
        session_version_ = v;
    }

    const std::string & get_net_type() const {
        return nettype_;
    }

    void set_net_type(const std::string & nt) {
        nettype_ = nt;
    }

    const std::string & get_addr_type() const {
        return addrtype_;
    }

    void set_addr_type(const std::string & at) {
        addrtype_ = at;
    }

    const std::string & get_unicast_address() const {
        return unicast_address_;
    }

    void set_unicast_address(const std::string & ua) {
        unicast_address_ = ua;
    }

    std::string to_string() const;
private:
    // <username> is the user's login on the originating host, or it is "-"
    //   if the originating host does not support the concept of user IDs.
    //   The <username> MUST NOT contain spaces.
    std::string username_;
    // <sess-id> is a numeric string such that the tuple of <username>,
    //   <sess-id>, <nettype>, <addrtype>, and <unicast-address> forms a
    //   globally unique identifier for the session.
    uint64_t session_id_;
    // <sess-version> is a version number for this session description.
    uint32_t session_version_;
    // <nettype> is a text string giving the type of network.  Initially
    //   "IN" is defined to have the meaning "Internet", but other values
    //   MAY be registered in the future
    std::string nettype_;
    // <addrtype> is a text string giving the type of the address that
    //   follows.  Initially "IP4" and "IP6" are defined, but other values
    //   MAY be registered in the future (see Section 8).
    std::string addrtype_;
    // <unicast-address> is the address of the machine from which the
    //   session was created.  For an address type of IP4, this is either
    //   the fully qualified domain name of the machine or the dotted-
    //   decimal representation of the IP version 4 address of the machine.
    //   For an address type of IP6, this is either the fully qualified
    //   domain name of the machine or the compressed textual
    //   representation of the IP version 6 address of the machine.  For
    //   both IP4 and IP6, the fully qualified domain name is the form that
    //   SHOULD be given unless this is unavailable, in which case the
    //   globally unique address MAY be substituted.
    //   For privacy reasons, it is sometimes desirable to obfuscate(故意混淆) the
    //   username and IP address of the session originator.  If this is a
    //   concern, an arbitrary <username> and private <unicast-address> MAY be
    //   chosen to populate the "o=" field, provided that these are selected
    //   in a manner that does not affect the global uniqueness of the field.
    std::string unicast_address_;
    
};
};