#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <functional>
#include <string>
#include <lua.hpp>

template <typename T>
struct FieldDesc {
    const char *name;
    void (*get)(lua_State *L, T &o);
    void (*set)(lua_State *L, T &o, int idx);
};

#define LUA_INT_FIELD(T, m) { #m, \
    [](lua_State *L, T &o) { lua_pushinteger(L, static_cast<lua_Integer>(o.m)); }, \
    [](lua_State *L, T &o, int idx) { o.m = static_cast<decltype(o.m)>(luaL_checkinteger(L, idx)); } }

#define LUA_NUM_FIELD(T, m) { #m, \
    [](lua_State *L, T &o) { lua_pushnumber(L, static_cast<lua_Number>(o.m)); }, \
    [](lua_State *L, T &o, int idx) { o.m = static_cast<decltype(o.m)>(luaL_checknumber(L, idx)); } }

#define LUA_BOOL_FIELD(T, m) { #m, \
    [](lua_State *L, T &o) { lua_pushboolean(L, o.m); }, \
    [](lua_State *L, T &o, int idx) { o.m = lua_toboolean(L, idx) != 0; } }

#define LUA_STR_FIELD(T, m) { #m, \
    [](lua_State *L, T &o) { lua_pushlstring(L, o.m.data(), o.m.size()); }, \
    [](lua_State *L, T &o, int idx) { std::size_t n; const char *s = luaL_checklstring(L, idx, &n); o.m.assign(s, n); } }


template <typename T>
struct LuaStruct;

template <typename T>
int structIndex(lua_State *L) {
    T &o = **static_cast<T **>(luaL_checkudata(L, 1, LuaStruct<T>::name));
    const char *key = luaL_checkstring(L, 2);
    for (const auto &f : LuaStruct<T>::fields) {
        if (std::strcmp(f.name, key) == 0) { f.get(L, o); return 1; }
    }
    return luaL_error(L, "%s has no field '%s'", LuaStruct<T>::name, key);
}

template <typename T>
int structNewIndex(lua_State *L) {
    T &o = **static_cast<T **>(luaL_checkudata(L, 1, LuaStruct<T>::name));
    const char *key = luaL_checkstring(L, 2);
    for (const auto &f : LuaStruct<T>::fields) {
        if (std::strcmp(f.name, key) == 0) { f.set(L, o, 3); return 0; }
    }
    return luaL_error(L, "%s has no field '%s'", LuaStruct<T>::name, key);
}

template <typename T>
void pushStruct(lua_State *L, T *p) {
    *static_cast<T **>(lua_newuserdata(L, sizeof(T *))) = p;
    if (luaL_newmetatable(L, LuaStruct<T>::name)) {
        lua_pushcclosure(L, structIndex<T>, 0);
        lua_setfield(L, -2, "__index");
        lua_pushcclosure(L, structNewIndex<T>, 0);
        lua_setfield(L, -2, "__newindex");
    }
    lua_setmetatable(L, -2);
}


template <typename T, std::size_t N>
int arrayIndex(lua_State *L) {
    auto &arr = **static_cast<std::array<T, N> **>(lua_touserdata(L, 1));
    lua_Integer i = luaL_checkinteger(L, 2);
    if (i < 0 || static_cast<std::size_t>(i) >= N) {
        return luaL_error(L, "index %d out of range [0, %d)", (int)i, (int)N);
    }
    pushStruct(L, &arr[static_cast<std::size_t>(i)]);
    return 1;
}

template <typename T, std::size_t N>
int arrayNewIndex(lua_State *L) {
    return luaL_error(L, "%s elements can't be replaced; set their fields instead", LuaStruct<T>::name);
}

template <typename T, std::size_t N>
int arrayLen(lua_State *L) {
    lua_pushinteger(L, static_cast<lua_Integer>(N));
    return 1;
}

template <typename T, std::size_t N>
void pushArray(lua_State *L, std::array<T, N> *arr, const char *meta) {
    *static_cast<std::array<T, N> **>(lua_newuserdata(L, sizeof(std::array<T, N> *))) = arr;
    if (luaL_newmetatable(L, meta)) {
        lua_pushcclosure(L, arrayIndex<T, N>, 0);
        lua_setfield(L, -2, "__index");
        lua_pushcclosure(L, arrayNewIndex<T, N>, 0);
        lua_setfield(L, -2, "__newindex");
        lua_pushcclosure(L, arrayLen<T, N>, 0);
        lua_setfield(L, -2, "__len");
    }
    lua_setmetatable(L, -2);
}


struct GlobalBinding {
    std::function<void(lua_State *)> push;
    std::function<void(lua_State *, int)> assign;
};

void registerLuaGlobalBinding(const char *name, GlobalBinding binding);
bool isGlobalBound(const char *name);
void initLuaGlobalBindings(lua_State *L);

template <typename T, std::size_t N>
void bindLuaGlobal(const char *name, std::array<T, N> &arr) {
    std::string key = name;
    std::string meta = "LuaArray:" + key;

    GlobalBinding b;
    b.push = [&arr, meta](lua_State *L) {
        pushArray(L, &arr, meta.c_str());
    };
    b.assign = [key](lua_State *L, int) {
        luaL_error(L, "'%s' is read-only", key.c_str());
    };
    registerLuaGlobalBinding(name, std::move(b));
}