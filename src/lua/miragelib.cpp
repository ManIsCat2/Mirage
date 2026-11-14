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
    return 0;
}

static int libSendChatMessage(lua_State *L) {
    size_t len = 0;
    const char *msg = luaL_checklstring(L, 1, &len);

    if (len >= MAX_CHAT_MSG_LENGTH) {
        len = MAX_CHAT_MSG_LENGTH - 1;
    }

    auto outPkt = Packet::createOutgoing(PacketType::CHAT, true);
    outPkt.write<uint8_t>(0);
    outPkt.write<uint16_t>(len);
    outPkt.write<std::string>(msg, len);
    outPkt.sendToAll();
    return 0;
}

static const luaL_Reg kMirageFuncs[] = {
    {"kickPlayer", libKickPlayer},
    {"sendChatMessage", libSendChatMessage},
    {nullptr, nullptr},
};

int luaopen_mirage(lua_State *L) {
    luaL_newlib(L, kMirageFuncs);
    lua_pushliteral(L, "0.1.0");
    lua_setfield(L, -2, "version");
    return 1;
}
