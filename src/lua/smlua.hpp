#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <lua.hpp>
#include "../mod.hpp"

enum class HookEventType {
    UPDATE,
    MARIO_UPDATE,
    BEFORE_MARIO_UPDATE,
    ON_SET_MARIO_ACTION,
    BEFORE_PHYS_STEP,
    ALLOW_PVP_ATTACK,
    ON_PVP_ATTACK,
    ON_PLAYER_CONNECTED,
    ON_PLAYER_DISCONNECTED,
    ON_HUD_RENDER,
    ALLOW_INTERACT,
    ON_INTERACT,
    ON_LEVEL_INIT,
    ON_WARP,
    ON_SYNC_VALID,
    ON_OBJECT_UNLOAD,
    ON_SYNC_OBJECT_UNLOAD,
    ON_PAUSE_EXIT,
    GET_STAR_COLLECTION_DIALOG,
    ON_SET_CAMERA_MODE,
    ON_OBJECT_RENDER,
    ON_DEATH,
    ON_PACKET_RECEIVE,
    USE_ACT_SELECT,
    ON_CHANGE_CAMERA_ANGLE,
    ON_SCREEN_TRANSITION,
    ALLOW_HAZARD_SURFACE,
    ON_CHAT_MESSAGE,
    OBJECT_SET_MODEL,
    CHARACTER_SOUND,
    BEFORE_SET_MARIO_ACTION,
    JOINED_GAME,
    ON_OBJECT_ANIM_UPDATE,
    ON_DIALOG,
    ON_EXIT,
    DIALOG_SOUND,
    ON_HUD_RENDER_BEHIND,
    ON_COLLIDE_LEVEL_BOUNDS,
    MIRROR_MARIO_RENDER,
    MARIO_OVERRIDE_PHYS_STEP_DEFACTO_SPEED,
    ON_OBJECT_LOAD,
    ON_PLAY_SOUND,
    ON_SEQ_LOAD,
    ON_ATTACK_OBJECT,
    ON_LANGUAGE_CHANGED,
    ON_MODS_LOADED,
    ON_NAMETAGS_RENDER,
    ON_DJUI_THEME_CHANGED,
    ON_GEO_PROCESS,
    BEFORE_GEO_PROCESS,
    ON_GEO_PROCESS_CHILDREN,
    MARIO_OVERRIDE_GEOMETRY_INPUTS,
    ON_INTERACTIONS,
    ALLOW_FORCE_WATER_ACTION,
    BEFORE_WARP,
    ON_INSTANT_WARP,
    MARIO_OVERRIDE_FLOOR_CLASS,
    ON_ADD_SURFACE,
    ON_CLEAR_AREAS,
    ON_PACKET_BYTESTRING_RECEIVE,
    ON_FIND_WALL_COLLISION,
    ON_FIND_CEIL,
    ON_FIND_FLOOR,
    ON_FIND_WATER_LEVEL,
    ON_FIND_POISON_GAS_LEVEL,
    ON_FIND_SURFACE_ON_RAY,
    ON_DYNOS_PACK_TOGGLED,
    BEFORE_PLAY_MODE_UPDATE,
    ON_PLAY_MODE_UPDATE,
    MAX,
};

struct LuaHook {
    int luaFuncRef = 0;
};

class SMLua {
public:
    SMLua();
    ~SMLua();

    bool init();
    void update();
    void shutdown();

    bool executeMod(const Mod &mod);
    void registerHook(HookEventType hookType, int funcRef);
    void registerFunc(lua_State *L, const char *name, int (*func)(lua_State *L));

    lua_State *getState() const { return L; }
private:
    lua_State *L = nullptr;
    std::unordered_map<HookEventType, std::vector<LuaHook>> registeredHooks;

    int createModEnv();
    void callHook(HookEventType hookType);
    template <typename F>
    void callHook(HookEventType hookType, F &&pushArgs);
};

extern SMLua gSMLua;