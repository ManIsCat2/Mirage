#include "globals.hpp"
#include "structs.hpp"
#include "../network/network.hpp"

template <>
struct LuaStruct<NetworkPlayer> {
    static constexpr const char *name = "NetworkPlayer";
    static constexpr FieldDesc<NetworkPlayer> fields[] = {
        LUA_FIELD(NetworkPlayer, connected),
        LUA_FIELD(NetworkPlayer, type),
        LUA_FIELD(NetworkPlayer, globalIndex),
        LUA_FIELD(NetworkPlayer, currLevelAreaSeqId),
        LUA_FIELD(NetworkPlayer, currCourseNum),
        LUA_FIELD(NetworkPlayer, currActNum),
        LUA_FIELD(NetworkPlayer, currLevelNum),
        LUA_FIELD(NetworkPlayer, currAreaIndex),
        LUA_FIELD(NetworkPlayer, currLevelSyncValid),
        LUA_FIELD(NetworkPlayer, currAreaSyncValid),
        LUA_FIELD(NetworkPlayer, networkId),
        LUA_FIELD(NetworkPlayer, lastReceived),
        LUA_FIELD(NetworkPlayer, lastSent),
        LUA_FIELD(NetworkPlayer, lastPingSent),
        LUA_FIELD(NetworkPlayer, modelIndex),
        LUA_FIELD(NetworkPlayer, ping),
        LUA_FIELD(NetworkPlayer, name),
        LUA_FIELD(NetworkPlayer, discordId),
    };
};

void registerLuaGlobals(void) {
    bindLuaGlobal("gNetworkPlayers", gNetworkPlayers);
}