#pragma once
#include <string>
namespace mms {
// 5.6.  Email Address and Phone Number ("e=" and "p=")

//       e=<email-address>
//       p=<phone-number>

//    The "e=" and "p=" lines specify contact information for the person
//    responsible for the conference.  This is not necessarily the same
//    person that created the conference announcement.

//    Inclusion of an email address or phone number is OPTIONAL.  Note that
//    the previous version of SDP specified that either an email field or a
//    phone field MUST be specified, but this was widely ignored.  The
//    change brings the specification into line with common usage.

//    If an email address or phone number is present, it MUST be specified
//    before the first media field.  More than one email or phone field can
//    be given for a session description.

//    Phone numbers SHOULD be given in the form of an international public
//    telecommunication number (see ITU-T Recommendation E.164) preceded by
//    a "+".  Spaces and hyphens may be used to split up a phone field to
//    aid readability if desired.  For example:

//       p=+1 617 555-6011

//    Both email addresses and phone numbers can have an OPTIONAL free text
//    string associated with them, normally giving the name of the person
//    who may be contacted.  This MUST be enclosed in parentheses if it is
//    present.  For example:

//       e=j.doe@example.com (Jane Doe)

//    The alternative RFC 2822 [29] name quoting convention is also allowed
//    for both email addresses and phone numbers.  For example:

//       e=Jane Doe <j.doe@example.com>
struct EmailAddress {
public:
    static std::string prefix;
    bool parse(const std::string & line);
    const std::string & get_address() const {
        return address;
    }

    void set_address(const std::string & addr) {
        address = addr;
    }

    std::string to_string() const;
public:
    std::string address;
};
};