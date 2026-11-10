#include "globals.hpp"
#include "structs.hpp"
#include "../network/network.hpp"

template <>
struct LuaStruct<NetworkPlayer> {
    static constexpr const char *name = "NetworkPlayer";
    static constexpr FieldDesc<NetworkPlayer> fields[] = {
        LUA_BOOL_FIELD(NetworkPlayer, connected),
        LUA_INT_FIELD(NetworkPlayer, type),
        LUA_INT_FIELD(NetworkPlayer, globalIndex),
        LUA_INT_FIELD(NetworkPlayer, currLevelAreaSeqId),
        LUA_INT_FIELD(NetworkPlayer, currCourseNum),
        LUA_INT_FIELD(NetworkPlayer, currActNum),
        LUA_INT_FIELD(NetworkPlayer, currLevelNum),
        LUA_INT_FIELD(NetworkPlayer, currAreaIndex),
        LUA_INT_FIELD(NetworkPlayer, currLevelSyncValid),
        LUA_INT_FIELD(NetworkPlayer, currAreaSyncValid),
        LUA_INT_FIELD(NetworkPlayer, networkId),
        LUA_NUM_FIELD(NetworkPlayer, lastReceived),
        LUA_NUM_FIELD(NetworkPlayer, lastSent),
        LUA_NUM_FIELD(NetworkPlayer, lastPingSent),
        LUA_INT_FIELD(NetworkPlayer, modelIndex),
        LUA_INT_FIELD(NetworkPlayer, ping),
        LUA_STR_FIELD(NetworkPlayer, name),
        LUA_STR_FIELD(NetworkPlayer, discordId),
    };
};

void registerLuaGlobals(void) {
    bindLuaGlobal("gNetworkPlayers", gNetworkPlayers);
}