#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <functional>
#include <string>
#include <type_traits>
#include <lua.hpp>

template <typename T>
inline constexpr bool isIntLike = (std::is_integral_v<T> && !std::is_same_v<T, bool>) || std::is_enum_v<T>;

inline void luaPush(lua_State *L, bool v) { lua_pushboolean(L, v); }
inline void luaPush(lua_State *L, const std::string& v) { lua_pushlstring(L, v.data(), v.size()); }

template <typename T, std::enable_if_t<isIntLike<T>, int> = 0>
void luaPush(lua_State *L, T v) { lua_pushinteger(L, static_cast<lua_Integer>(v)); }

template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
void luaPush(lua_State *L, T v) { lua_pushnumber(L, static_cast<lua_Number>(v)); }

template <typename T>
T luaTo(lua_State *L, int idx) {
    if constexpr(std::is_same_v<T, bool>) {
        return lua_toboolean(L, idx) != 0;
    } else if constexpr(std::is_same_v<T, std::string>) {
        size_t n;
        const char *s = luaL_checklstring(L, idx, &n);
        return std::string(s, n);
    } else if constexpr(std::is_floating_point_v<T>) {
        return static_cast<T>(luaL_checknumber(L, idx));
    } else {
        return static_cast<T>(luaL_checkinteger(L, idx));
    }
}

inline void setMetaFn(lua_State *L, const char *key, lua_CFunction fn) {
    lua_pushcclosure(L, fn, 0);
    lua_setfield(L, -2, key);
}

template <typename T>
void pushRef(lua_State *L, T* p, const char *meta, lua_CFunction get, lua_CFunction set, lua_CFunction len = nullptr) {
    *static_cast<T**>(lua_newuserdata(L, sizeof(T*))) = p;
    if (luaL_newmetatable(L, meta)) {
        setMetaFn(L, "__index", get);
        setMetaFn(L, "__newindex", set);
        if (len) setMetaFn(L, "__len", len);
    }
    lua_setmetatable(L, -2);
}

template <typename T>
T &checkRef(lua_State *L, int idx, const char *meta) {
    return **static_cast<T**>(luaL_checkudata(L, idx, meta));
}

inline std::size_t checkIndex(lua_State *L, int idx, std::size_t n) {
    lua_Integer i = luaL_checkinteger(L, idx);
    if (i < 0 || static_cast<std::size_t>(i) >= n) {
        luaL_error(L, "index %d out of range [0, %d)", (int)i, (int)n);
    }
    return static_cast<std::size_t>(i);
}

template <typename T>
struct LuaStruct;

template <typename T>
struct FieldDesc {
    const char *name;
    void (*get)(lua_State *L, T &obj);
    void (*set)(lua_State *L, T &obj, int idx);
};

#define LUA_FIELD(T, m) { #m, \
    [](lua_State *L, T &o) { luaPush(L, o.m); }, \
    [](lua_State *L, T &o, int idx) { o.m = luaTo<decltype(o.m)>(L, idx); } }

template <typename T, typename = void>
struct IsStruct : std::false_type {};
template <typename T>
struct IsStruct<T, std::void_t<decltype(LuaStruct<T>::name)>> : std::true_type {};

template <typename T>
int structGet(lua_State *L) {
    T &obj = checkRef<T>(L, 1, LuaStruct<T>::name);
    const char *key = luaL_checkstring(L, 2);
    for (const auto &f : LuaStruct<T>::fields) {
        if (std::strcmp(f.name, key) == 0) { f.get(L, obj); return 1; }
    }
    return luaL_error(L, "%s has no field '%s'", LuaStruct<T>::name, key);
}

template <typename T>
int structSet(lua_State *L) {
    T &obj = checkRef<T>(L, 1, LuaStruct<T>::name);
    const char *key = luaL_checkstring(L, 2);
    for (const auto &f : LuaStruct<T>::fields) {
        if (std::strcmp(f.name, key) == 0) { f.set(L, obj, 3); return 0; }
    }
    return luaL_error(L, "%s has no field '%s'", LuaStruct<T>::name, key);
}

template <typename T>
void pushStruct(lua_State *L, T* obj) {
    pushRef(L, obj, LuaStruct<T>::name, structGet<T>, structSet<T>);
}

template <typename T>
struct IsStdArray : std::false_type {};
template <typename E, std::size_t N>
struct IsStdArray<std::array<E, N>> : std::true_type {};

template <typename E>
void pushElement(lua_State *L, E *p) {
    if constexpr(IsStruct<E>::value) {
        pushStruct(L, p);
    } else {
        luaPush(L, *p);
    }
}

template <typename E, std::size_t N>
std::array<E, N> &arrayRef(lua_State *L) {
    return **static_cast<std::array<E, N> **>(lua_touserdata(L, 1));
}

template <typename E, std::size_t N>
int arrayGet(lua_State *L) {
    auto &arr = arrayRef<E, N>(L);
    pushElement(L, &arr[checkIndex(L, 2, N)]);
    return 1;
}

template <typename E, std::size_t N>
int arraySet(lua_State *L) {
    if constexpr(IsStruct<E>::value) {
        return luaL_error(L, "can't replace %s elements; set their fields instead", LuaStruct<E>::name);
    } else {
        auto &arr = arrayRef<E, N>(L);
        arr[checkIndex(L, 2, N)] = luaTo<E>(L, 3);
        return 0;
    }
}

template <typename E, std::size_t N>
int arrayLen(lua_State *L) {
    lua_pushinteger(L, static_cast<lua_Integer>(N));
    return 1;
}

template <typename E, std::size_t N>
void pushArray(lua_State *L, std::array<E, N>* arr, const char *meta) {
    pushRef(L, arr, meta, arrayGet<E, N>, arraySet<E, N>, arrayLen<E, N>);
}

struct GlobalBinding {
    std::function<void(lua_State*)> push;
    std::function<void(lua_State*, int)> assign;
};

void registerLuaGlobalBinding(const char *name, GlobalBinding binding);
bool isGlobalBound(const char *name);
void initLuaGlobalBindings(lua_State *L);

template <typename T>
void bindLuaGlobal(const char *name, T &var) {
    GlobalBinding b;
    b.push = [&var, name](lua_State *L) {
        if constexpr(IsStdArray<T>::value) {
            pushArray(L, &var, (std::string("LuaArray:") + name).c_str());
        } else {
            pushElement(L, &var);
        }
    };
    b.assign = [&var, name](lua_State *L, int idx) {
        if constexpr(IsStruct<T>::value || IsStdArray<T>::value) {
            luaL_error(L, "%s can't be replaced, assign its fields or elements instead", name);
        } else {
            var = luaTo<T>(L, idx);
        }
    };
    registerLuaGlobalBinding(name, std::move(b));
}