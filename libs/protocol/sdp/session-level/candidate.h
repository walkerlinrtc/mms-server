#pragma once
#include <string>
#include <unordered_map>
#include <cstdint>
/*
15.1.  "candidate" Attribute

   The candidate attribute is a media-level attribute only.  It contains
   a transport address for a candidate that can be used for connectivity
   checks.

   The syntax of this attribute is defined using Augmented BNF as
   defined in RFC 5234 [RFC5234]:

   candidate-attribute   = "candidate" ":" foundation SP component-id SP
                           transport SP
                           priority SP
                           connection-address SP     ;from RFC 4566
                           port         ;port from RFC 4566
                           SP cand-type
                           [SP rel-addr]
                           [SP rel-port]
                           *(SP extension-att-name SP
                                extension-att-value)

   foundation            = 1*32ice-char
   component-id          = 1*5DIGIT
   transport             = "UDP" / transport-extension
   transport-extension   = token              ; from RFC 3261
   priority              = 1*10DIGIT
   cand-type             = "typ" SP candidate-types
   candidate-types       = "host" / "srflx" / "prflx" / "relay" / token
   rel-addr              = "raddr" SP connection-address
   rel-port              = "rport" SP port
   extension-att-name    = byte-string    ;from RFC 4566
   extension-att-value   = byte-string
   ice-char              = ALPHA / DIGIT / "+" / "/"

   This grammar encodes the primary information about a candidate: its
   IP address, port and transport protocol, and its properties: the
   foundation, component ID, priority, type, and related transport
   address:

   <connection-address>:  is taken from RFC 4566 [RFC4566].  It is the
      IP address of the candidate, allowing for IPv4 addresses, IPv6
      addresses, and fully qualified domain names (FQDNs).  When parsing
      this field, an agent can differentiate an IPv4 address and an IPv6



Rosenberg                    Standards Track                   [Page 73]

RFC 5245                           ICE                        April 2010


      address by presence of a colon in its value - the presence of a
      colon indicates IPv6.  An agent MUST ignore candidate lines that
      include candidates with IP address versions that are not supported
      or recognized.  An IP address SHOULD be used, but an FQDN MAY be
      used in place of an IP address.  In that case, when receiving an
      offer or answer containing an FQDN in an a=candidate attribute,
      the FQDN is looked up in the DNS first using an AAAA record
      (assuming the agent supports IPv6), and if no result is found or
      the agent only supports IPv4, using an A.  If the DNS query
      returns more than one IP address, one is chosen, and then used for
      the remainder of ICE processing.

   <port>:  is also taken from RFC 4566 [RFC4566].  It is the port of
      the candidate.

   <transport>:  indicates the transport protocol for the candidate.
      This specification only defines UDP.  However, extensibility is
      provided to allow for future transport protocols to be used with
      ICE, such as TCP or the Datagram Congestion Control Protocol
      (DCCP) [RFC4340].

   <foundation>:  is composed of 1 to 32 <ice-char>s.  It is an
      identifier that is equivalent for two candidates that are of the
      same type, share the same base, and come from the same STUN
      server.  The foundation is used to optimize ICE performance in the
      Frozen algorithm.

   <component-id>:  is a positive integer between 1 and 256 that
      identifies the specific component of the media stream for which
      this is a candidate.  It MUST start at 1 and MUST increment by 1
      for each component of a particular candidate.  For media streams
      based on RTP, candidates for the actual RTP media MUST have a
      component ID of 1, and candidates for RTCP MUST have a component
      ID of 2.  Other types of media streams that require multiple
      components MUST develop specifications that define the mapping of
      components to component IDs.  See Section 14 for additional
      discussion on extending ICE to new media streams.

   <priority>:  is a positive integer between 1 and (2**31 - 1).

   <cand-type>:  encodes the type of candidate.  This specification
      defines the values "host", "srflx", "prflx", and "relay" for host,
      server reflexive, peer reflexive, and relayed candidates,
      respectively.  The set of candidate types is extensible for the
      future.






Rosenberg                    Standards Track                   [Page 74]

RFC 5245                           ICE                        April 2010


   <rel-addr> and <rel-port>:  convey transport addresses related to the
      candidate, useful for diagnostics and other purposes. <rel-addr>
      and <rel-port> MUST be present for server reflexive, peer
      reflexive, and relayed candidates.  If a candidate is server or
      peer reflexive, <rel-addr> and <rel-port> are equal to the base
      for that server or peer reflexive candidate.  If the candidate is
      relayed, <rel-addr> and <rel-port> is equal to the mapped address
      in the Allocate response that provided the client with that
      relayed candidate (see Appendix B.3 for a discussion of its
      purpose).  If the candidate is a host candidate, <rel-addr> and
      <rel-port> MUST be omitted.

   The candidate attribute can itself be extended.  The grammar allows
   for new name/value pairs to be added at the end of the attribute.  An
   implementation MUST ignore any name/value pairs it doesn't
   understand.

*/
namespace mms {
class Candidate {
public:
    enum CANDIDATE_TYPE {
        CAND_TYPE_HOST = 0,
        CAND_TYPE_SRFLX = 1,
        CAND_TYPE_PRFLX = 2,
        CAND_TYPE_RELAY = 3,
    };

    static std::string prefix;

    Candidate() = default;
    Candidate(const std::string & foundation, uint32_t component_id, const std::string & transport, uint32_t priority, const std::string & address, uint16_t port, CANDIDATE_TYPE cand_type, const std::string & rel_addr, uint16_t rel_port, const std::unordered_map<std::string, std::string> & ext): foundation_(foundation), component_id_(component_id), transport_(transport), priority_(priority), address_(address), port_(port), cand_type_(cand_type), rel_addr_(rel_addr), rel_port_(rel_port), exts_(ext) {

    }

    bool parse(const std::string & line);
    std::string to_string() const;
private:
    std::string foundation_;
    uint32_t component_id_;
    std::string transport_;
    uint32_t priority_;
    std::string address_;
    uint16_t port_;
    // typ
    CANDIDATE_TYPE cand_type_;
    // std::string cand_type; //"host" / "srflx" / "prflx" / "relay"
    std::string rel_addr_;
    uint16_t rel_port_;
    std::unordered_map<std::string, std::string> exts_;
};
};