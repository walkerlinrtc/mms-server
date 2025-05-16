#include "tls_id.h"
#include <sstream>
#include "base/utils/utils.h"
using namespace mms;

std::string TlsIdAttr::prefix = "a=tls-id:";
bool TlsIdAttr::parse(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    id = line.substr(prefix.size(), end_pos - prefix.size());
    return true;
}

std::string TlsIdAttr::to_string() const {
    std::ostringstream oss;
    oss << prefix << id << std::endl;
    return oss.str();
}