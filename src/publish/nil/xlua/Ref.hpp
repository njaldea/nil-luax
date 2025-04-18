#pragma once

extern "C"
{
#include "lauxlib.h"
}

namespace nil::xlua
{
    class Ref
    {
    public:
        explicit Ref(lua_State* init_state)
            : state(init_state)
            , ref(luaL_ref(state, LUA_REGISTRYINDEX))
        {
        }

        Ref(Ref&&) = delete;
        Ref(const Ref&) = delete;
        Ref& operator=(Ref&&) = delete;
        Ref& operator=(const Ref&) = delete;

        ~Ref()
        {
            luaL_unref(state, LUA_REGISTRYINDEX, ref);
        }

        lua_State* push()
        {
            lua_rawgeti(state, LUA_REGISTRYINDEX, ref);
            return state;
        }

    private:
        lua_State* state;
        int ref;
    };
}
