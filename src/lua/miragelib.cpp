#include "miragelib.hpp"
#include "../network/network.hpp"
#include "util.hpp"
#include <string>

static int libGetVersion(lua_State *L) {
    lua_pushliteral(L, "0.1.0");
    return 1;
}

static int libGetCoopNetId(lua_State *L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    if (index < 0 || index >= MAX_PLAYERS) {
        return luaL_error(L, "player index %d out of range", (int)index);
    }

    uint64_t peer = gNetworkPlayers[(size_t)index].endpoint.peerId;
    uint64_t coopNetId = gCoopNetIds[peer];

    std::string id = std::to_string(coopNetId);
    lua_pushlstring(L, id.c_str(), id.size());
    return 1;
}

static const luaL_Reg kMirageFuncs[] = {
    {"getVersion",      libGetVersion},
    {"getCoopNetId",      libGetCoopNetId},
    {nullptr, nullptr},
};

extern "C" int luaopen_mirage(lua_State *L) {
    luaL_newlib(L, kMirageFuncs);
    lua_pushliteral(L, "0.1.0");
    lua_setfield(L, -2, "version");
    return 1;
}
