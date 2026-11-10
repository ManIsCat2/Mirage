#include "miragelib.hpp"
#include "../network/network.hpp"
#include "../network/packet.hpp"
#include "util.hpp"
#include <string>

static int libKickPlayer(lua_State *L) {
    lua_Integer index = luaL_checkinteger(L, 1);
    if (!isValidGlobalIndex(index)) {
        return luaL_error(L, "player index %d out of range", (int)index);
    }

    auto outPkt = Packet::createOutgoing(PacketType::KICK, true);
    outPkt.write<uint8_t>(2);
    outPkt.sendTo(index);

    gNetworkPlayers[index].clear();

    outPkt = Packet::createOutgoing(PacketType::LEAVING, true);
    outPkt.write<uint8_t>(index);
    outPkt.sendToAll();
    return 1;
}

static const luaL_Reg kMirageFuncs[] = {
    {"kickPlayer", libKickPlayer},
    {nullptr, nullptr},
};

int luaopen_mirage(lua_State *L) {
    luaL_newlib(L, kMirageFuncs);
    lua_pushliteral(L, "0.1.0");
    lua_setfield(L, -2, "version");
    return 1;
}
