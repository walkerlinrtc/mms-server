#include <sstream>
#include <boost/algorithm/string.hpp>
#include "fingerprint.h"
#include "base/utils/utils.h"
using namespace mms;
std::string FingerPrint::prefix = "a=fingerprint:";
bool FingerPrint::parse(const std::string & line) {
    std::string::size_type end_pos = line.rfind("\r");
    if (end_pos == std::string::npos) {
        end_pos = line.size() - 1;
    }
    std::string valid_string = line.substr(prefix.size(), end_pos - prefix.size());

    std::vector<std::string> vs;
    boost::split(vs, valid_string, boost::is_any_of(" "));
    if (vs.size() != 2) {
        return false;
    }
    hash_name_ = vs[0];
    hash_val_ = vs[1];
    return true;
}

std::string FingerPrint::to_string() const {
    std::ostringstream oss;
    oss << prefix << hash_name_ << " " << hash_val_ << std::endl;
    return oss.str();
}