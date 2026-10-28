#pragma once

#include <cstdint>
#include "socket.hpp"

struct NetworkPlayer;

class Endpoint {
public:
    socket_t sock = INVALID_SOCKET;
    sockaddr_in addr{};
    uint64_t peerId = 0;

    static Endpoint fromSocket(socket_t s, const sockaddr_in &a);
    static Endpoint fromPeer(uint64_t id);

    bool isPeer() const { return peerId != 0; }
    bool isAllowed(void) const;

    bool operator==(const Endpoint &o) const;
    bool operator!=(const Endpoint &o) const { return !(*this == o); }

    NetworkPlayer *getPlayer();
};