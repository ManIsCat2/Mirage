#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <type_traits>
#include <algorithm>
#include <cstdint>
#include <chrono>
#include <map>
#include <list>
#include "endpoint.hpp"

constexpr size_t PACKET_LENGTH = 3000;
constexpr uint8_t PACKET_DESTINATION_BROADCAST = ((uint8_t)-1);
constexpr uint8_t PACKET_DESTINATION_SERVER = ((uint8_t)-2);
constexpr uint8_t MAX_CONFIG_STRING = 64;
constexpr size_t MAX_CHAT_MSG_LENGTH = 500;

enum class PacketType {
    ACK,
    PLAYER,
    OBJECT,
    SPAWN_OBJECTS,
    SPAWN_STAR,
    SPAWN_STAR_NLE,
    COLLECT_STAR,
    COLLECT_COIN,
    COLLECT_ITEM,
    GLOBAL_POPUP,
    DEBUG_SYNC,
    JOIN_REQUEST,
    JOIN,
    CHAT,
    KICK,
    KEEP_ALIVE,
    LEAVING,
    SAVE_FILE,
    SAVE_SET_FLAG,
    SAVE_REMOVE_FLAG,
    NETWORK_PLAYERS,
    DEATH,
    PING,
    PONG,
    UNUSED_23,
    CHANGE_LEVEL,
    CHANGE_AREA,
    LEVEL_AREA_REQUEST,
    LEVEL_REQUEST,
    LEVEL,
    AREA_REQUEST,
    AREA,
    SYNC_VALID,
    LEVEL_SPAWN_INFO,
    LEVEL_MACRO,
    LEVEL_AREA_INFORM,
    LEVEL_RESPAWN_INFO,
    CHANGE_WATER_LEVEL,
    PLAYER_SETTINGS,
    MOD_LIST_REQUEST,
    MOD_LIST,
    DOWNLOAD_REQUEST,
    DOWNLOAD,
    MOD_LIST_ENTRY,
    MOD_LIST_FILE,
    MOD_LIST_DONE,
    LUA_SYNC_TABLE_REQUEST,
    LUA_SYNC_TABLE,
    NETWORK_PLAYERS_REQUEST,
    REQUEST_FAILED,
    LUA_CUSTOM,
    LUA_CUSTOM_BYTESTRING,
    COMMAND,
    MODERATOR,
    CUSTOM = 255,
};

enum PacketLevelMatchType {
    PLMT_NONE,
    PLMT_AREA,
    PLMT_LEVEL
};

class Packet {
private:
    Endpoint sender;
    std::vector<uint8_t> data;
    size_t offset = 3;

    Packet(PacketType pType, bool reliable, uint8_t levelMatchType, int asGlobalIndex);

    std::vector<uint8_t> compress(void);
    void execute(void);
    void processOrdered(void);
    void setOrderedData(void);
public:
    PacketType type = PacketType::PLAYER;
    uint16_t seqId = 0;
    bool isReliable = false;
    uint8_t flags = 0;
    bool levelAreaMustMatch = false;
    bool requestBroadcast = false;
    bool isOrdered = false;
    bool levelMustMatch = false;
    uint8_t destGlobalId = 0;

    uint8_t orderedFromGlobalId = 0;
    uint16_t orderedGroupId = 0;
    uint16_t orderedSeqId = 0;

    uint8_t courseNum = 0;
    uint8_t actNum = 0;
    int16_t levelNum = 0;
    uint8_t areaIndex = 0;

    Packet(const Endpoint &from, const uint8_t *compData, size_t compLen);
    static Packet createOutgoing(PacketType pType, bool reliable = false, uint8_t levelMatchType = PLMT_NONE, int asGlobalIndex = 0);

    Packet duplicate(void);

    template<typename T>
    T read(size_t length = 0) {
        if constexpr(std::is_same_v<T, std::string>) {
            if (offset >= data.size()) return "";
            size_t available = data.size() - offset;
            size_t actualLen = std::min(length, available);
            std::string val(reinterpret_cast<const char*>(data.data() + offset), actualLen);
            offset += std::min(length, available);
            return val;
        } else {
            constexpr size_t sz = sizeof(T);
            if (offset + sz > data.size()) {
                offset = data.size();
                return T(0);
            }
            T val{};
            std::memcpy(&val, data.data() + offset, sz);
            offset += sz;
            return val;
        }
    }

    template<typename T>
    void write(const T &val, size_t length = 0) {
        if constexpr(std::is_same_v<T, std::string>) {
            size_t strLen = val.length();
            data.reserve(data.size() + length);
            for (size_t i = 0; i < length; ++i) {
                if (i < strLen) data.push_back(static_cast<uint8_t>(val[i]));
                else data.push_back(0x00);
            }
        } else {
            constexpr size_t sz = sizeof(T);
            data.reserve(data.size() + sz);
            const uint8_t *ptr = reinterpret_cast<const uint8_t*>(&val);
            for (size_t i = 0; i < sz; ++i) {
                data.push_back(ptr[i]);
            }
        }
    }

    std::string getSenderIdentifier(void);
    void sendTo(const Endpoint &dest);
    void sendTo(int globalIdx);
    void sendToAll(void);
    void sendToAllExcept(const Endpoint &skip);
    void handle(void);
};

struct ReliablePacket {
    uint16_t seqId;
    Endpoint dest;
    std::vector<uint8_t> compressedData;
    std::chrono::steady_clock::time_point lastSend;
    int sendAttempts;
};

struct OrderedState {
    uint16_t processSeqId = 1;
    std::map<uint16_t, Packet> queuedPackets;
};

extern std::list<ReliablePacket> gReliablePackets;

extern void packetOrderedBegin(void);
extern void packetOrderedEnd(void);