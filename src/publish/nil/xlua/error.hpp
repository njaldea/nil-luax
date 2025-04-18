#pragma once

extern "C"
{
#include <lua.h>
}

#include <stdexcept>

namespace nil::xlua
{
    inline void throw_error(lua_State* state)
    {
        throw std::invalid_argument("Error: " + std::string(lua_tostring(state, -1)));
    }
}
