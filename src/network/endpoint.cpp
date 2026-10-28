#include "endpoint.hpp"
#include "network.hpp"
#include "log.hpp"
#include "../config.hpp"

Endpoint Endpoint::fromSocket(socket_t s, const sockaddr_in &a) {
    Endpoint e;
    e.sock = s;
    e.addr = a;
    return e;
}

Endpoint Endpoint::fromPeer(uint64_t id) {
    Endpoint e;
    e.peerId = id;
    return e;
}

bool Endpoint::operator==(const Endpoint &o) const {
    if (isPeer() || o.isPeer()) return isPeer() && peerId == o.peerId;
    return sockaddrInEqual(addr, o.addr);
}

NetworkPlayer *Endpoint::getPlayer() {
    return isPeer() ? getNetworkPlayerFromPeerId(peerId) : getNetworkPlayerFromAddr(addr);
}

bool Endpoint::isAllowed(void) const {
    if (isPeer()) {
        if (gServerConfig.whitelist.size() > 0) {
            if (std::find(gServerConfig.whitelist.begin(), gServerConfig.whitelist.end(), gNetworkPlayerCoopNetIds[peerId]) != gServerConfig.whitelist.end()) {
            } else {
                return false;
            }
        }
        if (std::find(gServerConfig.blacklist.begin(), gServerConfig.blacklist.end(), gNetworkPlayerCoopNetIds[peerId]) != gServerConfig.blacklist.end()) {
            return false;
        }
    }
    return true;
}