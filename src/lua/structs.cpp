#include "structs.hpp"
#include <unordered_map>

// idk why i had to do this but if i didnt registerLuaGlobalBinding would
// throw floating point exception so yeah
static std::unordered_map<std::string, GlobalBinding> &luaBindings() {
    static std::unordered_map<std::string, GlobalBinding> table;
    return table;
}

static int globalIndex(lua_State *L) {
    const char *key = lua_tostring(L, 2);
    if (!key) return 0;
    auto &table = luaBindings();
    auto it = table.find(key);
    if (it == table.end()) return 0;
    it->second.push(L);
    return 1;
}

static int globalNewIndex(lua_State *L) {
    const char *key = lua_tostring(L, 2);
    if (key) {
        auto &table = luaBindings();
        auto it = table.find(key);
        if (it != table.end()) {
            it->second.assign(L, 3);
            return 0;
        }
    }
    lua_rawset(L, 1);
    return 0;
}

void registerLuaGlobalBinding(const char *name, GlobalBinding binding) {
    luaBindings()[name] = std::move(binding);
}

bool isGlobalBound(const char *name) {
    return luaBindings().count(name) != 0;
}

void initLuaGlobalBindings(lua_State *L) {
    lua_pushglobaltable(L);
    lua_newtable(L);
    lua_pushcclosure(L, globalIndex, 0);
    lua_setfield(L, -2, "__index");
    lua_pushcclosure(L, globalNewIndex, 0);
    lua_setfield(L, -2, "__newindex");
    lua_setmetatable(L, -2);
    lua_pop(L, 1);
}