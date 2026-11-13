#include "network.hpp"
#include "packet.hpp"
#include "log.hpp"
#include "../config.hpp"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <thread>

constexpr int MAX_SOCKET_RECV_PER_TICK = 256;
constexpr double NETWORK_PLAYER_PING_TIMEOUT = 3.0;
constexpr double NETWORK_PLAYER_TIMEOUT = 15.0;

std::array<NetworkPlayer, MAX_PLAYERS> gNetworkPlayers{};
std::map<uint64_t, uint64_t> gCoopNetIds{};
std::unique_ptr<NetworkSystem> gNetworkSystem = nullptr;

void NetworkPlayer::clear(void) {
    connected = false;
    type = NetworkPlayerType::UNK;
    globalIndex = 0;
    currLevelAreaSeqId = 0;
    currCourseNum = 0;
    currActNum = 0;
    currLevelNum = 0;
    currAreaIndex = 0;
    currLevelSyncValid = 0;
    currAreaSyncValid = 0;
    networkId = 0;
    lastReceived = 0.0f;
    lastSent = 0.0f;
    lastPingSent = 0.0f;
    modelIndex = 0;
    ping = 0;
    palette = {};
    name.clear();
    discordId.clear();

    if (endpoint.isPeer()) gCoopNetIds.erase(endpoint.peerId);
}

NetworkPlayer *NetworkPlayer::fromAddr(const sockaddr_in &a) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (gNetworkPlayers[i].connected && !gNetworkPlayers[i].endpoint.isPeer() && sockaddrInEqual(a, gNetworkPlayers[i].endpoint.addr)) {
            return &gNetworkPlayers[i];
        }
    }
    return nullptr;
}

NetworkPlayer *NetworkPlayer::fromPeer(uint64_t peerId) {
    if (peerId == 0) return nullptr;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (gNetworkPlayers[i].connected && gNetworkPlayers[i].endpoint.peerId == peerId && gNetworkPlayers[i].endpoint.isPeer()) {
            return &gNetworkPlayers[i];
        }
    }
    return nullptr;
}

NetworkPlayer *NetworkPlayer::fromLevel(int16_t courseNum, int16_t actNum, int16_t levelNum) {
    for (auto &np : gNetworkPlayers) {
        if (np.globalIndex == 0)           { continue; }
        if (!np.connected)                 { continue; }
        if (!np.currLevelSyncValid)        { continue; }
        if (np.currCourseNum != courseNum) { continue; }
        if (np.currActNum    != actNum)    { continue; }
        if (np.currLevelNum  != levelNum)  { continue; }
        return &np;
    }
    return nullptr;
}

NetworkPlayer *NetworkPlayer::fromLevelArea(int16_t courseNum, int16_t actNum, int16_t levelNum, int16_t areaIndex) {
    for (auto &np : gNetworkPlayers) {
        if (np.globalIndex == 0)           { continue; }
        if (!np.connected)                 { continue; }
        if (!np.currLevelSyncValid)        { continue; }
        if (!np.currAreaSyncValid)         { continue; }
        if (np.currCourseNum != courseNum) { continue; }
        if (np.currActNum    != actNum)    { continue; }
        if (np.currLevelNum  != levelNum)  { continue; }
        if (np.currAreaIndex != areaIndex) { continue; }
        return &np;
    }
    return nullptr;
}

std::string NetworkPlayer::getNameWithColor(void) {
    return name + "\\#reset\\";
}

void clearNetworkPlayers(void) {
    for (int i = 1; i < MAX_PLAYERS; i++) {
        NetworkPlayer *np = &gNetworkPlayers[i];
        np->clear();
    }
    gCoopNetIds.clear();
}

static void clearCoopNetPlayers(void) {
    for (int i = 1; i < MAX_PLAYERS; i++) {
        NetworkPlayer *np = &gNetworkPlayers[i];
        if (np->endpoint.isPeer()) np->clear();
    }
    gCoopNetIds.clear();
}

void NetworkSystem::sendToPlayer(int globalIndex, const uint8_t *data, size_t len) {
    if (globalIndex >= 0 && globalIndex < MAX_PLAYERS) {
        sendTo(gNetworkPlayers[globalIndex].endpoint, data, len);
    }
}

void NetworkSystem::sendToAll(const uint8_t *data, size_t len, int ignoreIndex) {
    for (int i = 1; i < MAX_PLAYERS; i++) {
        if (!gNetworkPlayers[i].connected || i == ignoreIndex) continue;
        sendTo(gNetworkPlayers[i].endpoint, data, len);
    }
}

void NetworkSystem::updateReliables(void) {
    auto now = std::chrono::steady_clock::now();
    auto it = gReliablePackets.begin();

    while (it != gReliablePackets.end()) {
        std::chrono::duration<float> elapsed = now - it->lastSend;
        float maxElapsed = std::min(0.07f * (it->sendAttempts * it->sendAttempts), 4.0f);

        if (elapsed.count() > maxElapsed) {
            sendTo(it->dest, it->compressedData.data(), it->compressedData.size());

            it->lastSend = now;
            it->sendAttempts++;

            if (it->sendAttempts >= 15) {
                Logging::info("SERVER", "giving up on reliable packet with seq {}", it->seqId);
                it = gReliablePackets.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void NetworkSystem::updatePlayers(void) {
    double now = clockElapsed();

    for (int i = 1; i < MAX_PLAYERS; i++) {
        NetworkPlayer *np = &gNetworkPlayers[i];
        if (!np->connected) { continue; }

        if (now - np->lastPingSent > NETWORK_PLAYER_PING_TIMEOUT) {
            auto pkt = Packet::createOutgoing(PacketType::PING, false);
            pkt.write<uint8_t>(np->globalIndex);
            pkt.write<double>(now);
            pkt.sendTo(np->globalIndex);
            np->lastPingSent = now;
        }
    }

    for (int i = 1; i < MAX_PLAYERS; i++) {
        NetworkPlayer *np = &gNetworkPlayers[i];
        if (!np->connected) { continue; }

        if (now - np->lastReceived > NETWORK_PLAYER_TIMEOUT) {
            Logging::info("SERVER", "dropping globalIndex {} for network inactivity", i);
            uint8_t globalIndex = gNetworkPlayers[i].globalIndex;
            Endpoint &ep = gNetworkPlayers[i].endpoint;

            auto leaving = Packet::createOutgoing(PacketType::LEAVING, true);
            leaving.write<uint8_t>(globalIndex);
            leaving.sendToAllExcept(ep);

            gReliablePackets.remove_if([&ep](const ReliablePacket &p) { return p.dest == ep; });
            gNetworkPlayers[i].clear();
            continue;
        }

        if (now - np->lastSent > NETWORK_PLAYER_TIMEOUT / 3.0) {
            auto pkt = Packet::createOutgoing(PacketType::KEEP_ALIVE, false);
            pkt.sendTo(np->globalIndex);
        }
    }
}

void NetworkSystem::updateNetwork(void) {
    updateReliables();
    updatePlayers();
}

bool SocketNetworkSystem::init(int port) {
    udpSock = new UDPSocket(port);
    Logging::info("NETWORK", "initialized socket on port {}", port);
    return true;
}

void SocketNetworkSystem::shutdown(void) {
    if (udpSock) {
        delete udpSock;
        udpSock = nullptr;
    }
}

void SocketNetworkSystem::poll(void) {
    if (!udpSock) return;
    uint8_t buffer[PACKET_LENGTH];
    for (int i = 0; i < MAX_SOCKET_RECV_PER_TICK; i++) {
        sockaddr_in clientAddr;
        std::memset(&clientAddr, 0, sizeof(clientAddr));
        ssize_t received = udpSock->receive(buffer, sizeof(buffer), clientAddr);
        if (received <= 0) break;
        Packet pkt(Endpoint::fromSocket(udpSock->getSock(), clientAddr), buffer, received);
        pkt.handle();
    }
}

void SocketNetworkSystem::sendTo(const Endpoint &dest, const uint8_t *data, size_t len) {
    if (!udpSock) return;
    sendto(udpSock->getSock(), reinterpret_cast<const char*>(data), len, 0, (struct sockaddr*)&dest.addr, sizeof(dest.addr));
}

void CoopNetNetworkSystem::onConnected(uint64_t userId) {
    Logging::info("NETWORK", "connected to coopnet as user {}", userId);
    gNetworkPlayers[0].endpoint = Endpoint::fromPeer(userId);

    CoopNetRc rc = coopnet_lobby_create(gServerConfig.game.c_str(), gServerConfig.version.c_str(), gServerConfig.name.c_str(), gServerConfig.mode.c_str(), gServerConfig.maxPlayers, gServerConfig.password.c_str(), gServerConfig.description.c_str());
    if (rc == COOPNET_OK) {
        Logging::info("NETWORK", "created coopnet lobby successfully");
    }
}

void CoopNetNetworkSystem::onDisconnected(bool intentional) {
    Logging::info("NETWORK", "disconnected from coopnet, intentional: {}", intentional);
    clearCoopNetPlayers();
}

void CoopNetNetworkSystem::onReceive(uint64_t userId, const uint8_t *data, uint64_t dataLength) {
    Packet pkt(Endpoint::fromPeer(userId), data, dataLength);
    pkt.handle();
}

void CoopNetNetworkSystem::onLobbyJoined(uint64_t lobbyId, uint64_t userId, uint64_t ownerId, uint64_t destId) {
    Logging::info("NETWORK", "joined lobby id {} with dest {}", lobbyId, destId);
    gCoopNetIds[userId] = destId;
}

void CoopNetNetworkSystem::onLobbyLeft(uint64_t lobbyId, uint64_t userId) {
    Logging::info("NETWORK", "user {} left lobby {}", userId, lobbyId);
}

void CoopNetNetworkSystem::onError(enum MPacketErrorNumber error, uint64_t tag) {
    Logging::error("NETWORK", "coopnet error: {} tag: {}", (int)error, tag);
}

void CoopNetNetworkSystem::onPeerConnect(uint64_t peerId) {
    Logging::info("NETWORK", "peer {} connected", peerId);
}

void CoopNetNetworkSystem::onPeerDisconnect(uint64_t peerId) {
    Logging::info("NETWORK", "peer {} disconnected", peerId);
    NetworkPlayer *np = NetworkPlayer::fromPeer(peerId);
    if (np) {
        uint8_t idx = np->globalIndex;
        Logging::info("NETWORK", "player {} disconnected via coopnet peer drop", np->name);
        auto leaving = Packet::createOutgoing(PacketType::LEAVING, true);
        leaving.write<uint8_t>(idx);
        leaving.sendToAll();
        gNetworkPlayers[idx].clear();
    }
}

void CoopNetNetworkSystem::onLoadBalance(const char *host, uint32_t port) {
    // does this even do anything?
}

bool CoopNetNetworkSystem::init(int port) {
    (void)port;
    gCoopNetCallbacks.OnConnected = CoopNetNetworkSystem::onConnected;
    gCoopNetCallbacks.OnDisconnected = CoopNetNetworkSystem::onDisconnected;
    gCoopNetCallbacks.OnReceive = CoopNetNetworkSystem::onReceive;
    gCoopNetCallbacks.OnLobbyJoined = CoopNetNetworkSystem::onLobbyJoined;
    gCoopNetCallbacks.OnLobbyLeft = CoopNetNetworkSystem::onLobbyLeft;
    gCoopNetCallbacks.OnError = CoopNetNetworkSystem::onError;
    gCoopNetCallbacks.OnPeerConnected = CoopNetNetworkSystem::onPeerConnect;
    gCoopNetCallbacks.OnPeerDisconnected = CoopNetNetworkSystem::onPeerDisconnect;
    gCoopNetCallbacks.OnLoadBalance = CoopNetNetworkSystem::onLoadBalance;

    CoopNetRc rc = coopnet_begin("net.coop64.us", 34197, gServerConfig.name.c_str(), 0);
    if (rc == COOPNET_OK) {
        Logging::info("NETWORK", "initialized coopnet");
        needsLobbyCreate = true;
        return true;
    }
    Logging::error("NETWORK", "failed to initialize coopnet");
    return false;
}

void CoopNetNetworkSystem::shutdown(void) {
    coopnet_shutdown();
}

void CoopNetNetworkSystem::poll(void) {
    coopnet_update();
}

void CoopNetNetworkSystem::sendTo(const Endpoint &dest, const uint8_t *data, size_t len) {
    if (dest.isPeer()) coopnet_send_to(dest.peerId, data, len);
}

HybridNetworkSystem::HybridNetworkSystem() : socketSys(std::make_unique<SocketNetworkSystem>()), coopSys(std::make_unique<CoopNetNetworkSystem>()) {}

bool HybridNetworkSystem::init(int port) {
    if (!socketSys->init(port)) {
        return false;
    }

    if (!coopSys->init(port)) {
        return false;
    }
    return true;
}

void HybridNetworkSystem::shutdown(void) {
    coopSys->shutdown();
    socketSys->shutdown();
}

void HybridNetworkSystem::poll(void) {
    socketSys->poll();
    coopSys->poll();
}

void HybridNetworkSystem::sendTo(const Endpoint &dest, const uint8_t *data, size_t len) {
    if (dest.isPeer()) {
        coopSys->sendTo(dest, data, len);
    } else {
        socketSys->sendTo(dest, data, len);
    }
}

bool networkInit(NetworkSystemType type, int port) {
    if (type == NetworkSystemType::SOCKET) {
        gNetworkSystem = std::make_unique<SocketNetworkSystem>();
    } else if (type == NetworkSystemType::COOPNET) {
        gNetworkSystem = std::make_unique<CoopNetNetworkSystem>();
    } else if (type == NetworkSystemType::HYBRID) {
        gNetworkSystem = std::make_unique<HybridNetworkSystem>();
    } else {
        return false;
    }

    return gNetworkSystem->init(port);
}

void networkShutdown(void) {
    if (gNetworkSystem) {
        Logging::info("SERVER", "shutting down");
        gNetworkSystem->shutdown();
        gNetworkSystem.reset();
    }
    clearNetworkPlayers();
}
