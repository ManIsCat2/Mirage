#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <array>
#include <memory>
#include "socket.hpp"
#include "endpoint.hpp"
#include "libcoopnet.h"

constexpr int MAX_PLAYERS = 16;

enum NetworkSystemType {
    SYS_SOCKET = 0,
    SYS_COOPNET = 1,
    SYS_BOTH = 2
};

struct PlayerPalette {
    uint8_t colors[24];
};

struct NetworkPlayer {
    bool connected = false;
    uint8_t type = 0;
    uint8_t globalIndex = 0;
    uint16_t currLevelAreaSeqId = 0;
    int16_t currCourseNum = 0;
    int16_t currActNum = 0;
    int16_t currLevelNum = 0;
    int16_t currAreaIndex = 0;
    uint8_t currLevelSyncValid = 0;
    uint8_t currAreaSyncValid = 0;
    int64_t networkId = 0;
    uint8_t modelIndex = 0;
    uint32_t ping = 0;
    PlayerPalette palette{};
    std::string name;
    std::string discordId;
};

enum NetworkPlayerType {
    NPT_UNKNOWN,
    NPT_LOCAL,
    NPT_SERVER,
    NPT_CLIENT,
};

extern std::array<NetworkPlayer, MAX_PLAYERS> gNetworkPlayers;
extern std::array<Endpoint, MAX_PLAYERS> gNetworkPlayerEndpoints;
extern std::map<uint64_t, uint64_t> gNetworkPlayerCoopNetIds;
extern void clearNetworkPlayer(int globalIndex);
extern void clearNetworkPlayers();

class NetworkSystem {
public:
    virtual ~NetworkSystem() = default;

    virtual bool init(int port) = 0;
    virtual void shutdown() = 0;
    virtual void update() = 0;
    virtual void sendTo(const Endpoint &dest, const uint8_t *data, size_t len) = 0;
    virtual bool requireServerBroadcast() const = 0;

    void sendToPlayer(int globalIndex, const uint8_t *data, size_t len);
    void sendToAll(const uint8_t *data, size_t len, int ignoreIndex = -1);
protected:
    void updateReliables();
};

class SocketNetworkSystem : public NetworkSystem {
private:
    UDPSocket *udpSock = nullptr;
public:
    bool init(int port) override;
    void shutdown() override;
    void update() override;
    void sendTo(const Endpoint &dest, const uint8_t *data, size_t len) override;
    bool requireServerBroadcast() const override { return true; }

    void pollSocket();
    UDPSocket *getUdpSocket() { return udpSock; }
};

class CoopNetNetworkSystem : public NetworkSystem {
private:
    uint64_t localUserId = 0;
    uint64_t localLobbyId = 0;
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
    void shutdown() override;
    void update() override;
    void sendTo(const Endpoint &dest, const uint8_t *data, size_t len) override;
    bool requireServerBroadcast() const override { return false; }

    void pollCoopNet();

    void setLocalUserId(uint64_t id) { localUserId = id; }
    void setLocalLobbyId(uint64_t id) { localLobbyId = id; }
    uint64_t getLocalLobbyId() { return localLobbyId; }
    uint64_t getLocalUserId() { return localUserId; }
};

class HybridNetworkSystem : public NetworkSystem {
private:
    std::unique_ptr<SocketNetworkSystem> socketSys;
    std::unique_ptr<CoopNetNetworkSystem> coopSys;
    bool coopEnabled = false;
public:
    HybridNetworkSystem();
    bool init(int port) override;
    void shutdown() override;
    void update() override;
    void sendTo(const Endpoint &dest, const uint8_t *data, size_t len) override;
    bool requireServerBroadcast() const override { return true; }
};

extern std::unique_ptr<NetworkSystem> gNetworkSystem;
extern NetworkPlayer *getNetworkPlayerFromLevel(int16_t courseNum, int16_t actNum, int16_t levelNum);
extern NetworkPlayer *getNetworkPlayerFromArea(int16_t courseNum, int16_t actNum, int16_t levelNum, int16_t areaIndex);
extern NetworkPlayer *getNetworkPlayerFromAddr(const sockaddr_in &a);
extern NetworkPlayer *getNetworkPlayerFromPeerId(uint64_t peerId);

extern std::string getNetworkPlayerName(int globalIndex);

extern bool networkInit(NetworkSystemType type, int port);
extern void networkShutdown();