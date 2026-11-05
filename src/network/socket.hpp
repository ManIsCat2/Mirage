#pragma once

#include <cstdint>
#include <cstddef>
#include "util.hpp"

class UDPSocket {
private:
    socket_t sock;
public:
    UDPSocket(int port);
    ~UDPSocket();
    
    int getSock() const;
    ssize_t receive(uint8_t *buffer, size_t maxLen, sockaddr_in &clientAddr);
};

extern bool sockaddrInEqual(const sockaddr_in &a, const sockaddr_in &b);
