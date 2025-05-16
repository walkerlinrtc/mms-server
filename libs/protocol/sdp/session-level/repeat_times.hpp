#pragma once
#include <string>
namespace mms {
// 5.10.  Repeat Times ("r=")
//       r=<repeat interval> <active duration> <offsets from start-time>
struct RepeatTimes {
public:
    std::string raw_string;
    std::string repeat_interval;
    std::string active_duration;
    std::string offset;
};
};