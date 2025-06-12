#pragma once
#include <cstddef>
namespace mms {
class SocketTrafficObserver {
public:
    virtual void on_bytes_in(size_t bytes) = 0;
    virtual void on_bytes_out(size_t bytes) = 0;
    virtual ~SocketTrafficObserver() = default;
};
};