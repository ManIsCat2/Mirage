#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <array>
#include <memory>
#include "socket.hpp"
#include "endpoint.hpp"
#include "util.hpp"
#include "libcoopnet.h"

enum class NetworkSystemType {
    SOCKET = 0,
    COOPNET = 1,
    HYBRID = 2
};

enum class NetworkPlayerType {
    UNK,
    LOCAL,
    SERVER,
    CLIENT,
};

struct PlayerPalette {
    uint8_t colors[24];
};

struct NetworkPlayer {
    bool connected = false;
    NetworkPlayerType type = NetworkPlayerType::UNK;
    uint8_t globalIndex = 0;
    uint16_t currLevelAreaSeqId = 0;
    int16_t currCourseNum = 0;
    int16_t currActNum = 0;
    int16_t currLevelNum = 0;
    int16_t currAreaIndex = 0;
    uint8_t currLevelSyncValid = 0;
    uint8_t currAreaSyncValid = 0;
    int64_t networkId = 0;
    float lastReceived;
    float lastSent;
    float lastPingSent;
    uint8_t modelIndex = 0;
    uint32_t ping = 0;
    PlayerPalette palette{};
    std::string name;
    std::string discordId;

    Endpoint endpoint;

    static NetworkPlayer *fromAddr(const sockaddr_in &a);
    static NetworkPlayer *fromPeer(uint64_t peerId);
    static NetworkPlayer *fromLevel(int16_t courseNum, int16_t actNum, int16_t levelNum);
    static NetworkPlayer *fromLevelArea(int16_t courseNum, int16_t actNum, int16_t levelNum, int16_t areaIndex);
    std::string getNameWithColor(void);
    void clear(void);
};

class NetworkSystem {
public:
    virtual ~NetworkSystem() = default;

    virtual bool init(int port) = 0;
    virtual void shutdown(void) = 0;
    virtual void poll(void) = 0;
    virtual void sendTo(const Endpoint &dest, const uint8_t *data, size_t len) = 0;
    virtual bool requireServerBroadcast(void) const = 0;

    void sendToPlayer(int globalIndex, const uint8_t *data, size_t len);
    void sendToAll(const uint8_t *data, size_t len, int ignoreIndex = -1);
    void updateNetwork(void);
protected:
    void updateReliables(void);
    void updatePlayers(void);
};

class SocketNetworkSystem : public NetworkSystem {
private:
    UDPSocket *udpSock = nullptr;
public:
    bool init(int port) override;
    void shutdown(void) override;
    void poll(void) override;
    void sendTo(const Endpoint &dest, const uint8_t *data, size_t len) override;
    bool requireServerBroadcast(void) const override { return true; }

    UDPSocket *getUdpSocket() { return udpSock; }
};

class CoopNetNetworkSystem : public NetworkSystem {
private:
    bool needsLobbyCreate = false;

    static void onConnected(uint64_t userId);
    static void onDisconnected(bool intentional);
    static void onReceive(uint64_t userId, const uint8_t *data, uint64_t dataLength);
    static void onLobbyJoined(uint64_t lobbyId, uint64_t userId, uint64_t ownerId, uint64_t destId);
    static void onLobbyLeft(uint64_t lobbyId, uint64_t userId);
    static void onError(enum MPacketErrorNumber error, uint64_t tag);
    static void onPeerConnect(uint64_t peerId);
    static void onPeerDisconnect(uint64_t peerId);
    static void onLoadBalance(const char *host, uint32_t port);
public:
    bool init(int port) override;
    void shutdown(void) override;
    void poll(void) override;
    void sendTo(const Endpoint &dest, const uint8_t *data, size_t len) override;
    bool requireServerBroadcast(void) const override { return false; }
};

class HybridNetworkSystem : public NetworkSystem {
private:
    std::unique_ptr<SocketNetworkSystem> socketSys;
    std::unique_ptr<CoopNetNetworkSystem> coopSys;
public:
    HybridNetworkSystem();
    bool init(int port) override;
    void shutdown(void) override;
    void poll() override;
    void sendTo(const Endpoint &dest, const uint8_t *data, size_t len) override;
    bool requireServerBroadcast(void) const override { return true; }
};

extern std::array<NetworkPlayer, MAX_PLAYERS> gNetworkPlayers;
extern std::map<uint64_t, uint64_t> gCoopNetIds;
extern std::unique_ptr<NetworkSystem> gNetworkSystem;

extern void clearNetworkPlayers(void);
extern bool networkInit(NetworkSystemType type, int port);
extern void networkShutdown(void);
