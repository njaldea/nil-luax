#pragma once

#include "TypeDef.hpp"
#include "Var.hpp"
#include "error.hpp"

#include <nil/xalt/fn_sign.hpp>
#include <nil/xalt/tlist.hpp>

extern "C"
{
#include "lualib.h"
}

#include <string_view>

namespace nil::xlua
{
    class State final
    {
    public:
        State()
            : state(luaL_newstate())
        {
            luaL_openlibs(state);
        }

        State(State&&) = default;
        State& operator=(State&&) = default;

        State(const State&) = delete;
        State& operator=(const State&) = delete;

        ~State() noexcept
        {
            if (state != nullptr)
            {
                lua_close(state);
            }
        }

        void load(std::string_view path)
        {
            if (luaL_dofile(state, path.data()) != LUA_OK)
            {
                throw_error(state);
            }
        }

        void run(std::string_view script)
        {
            if (luaL_dostring(state, script.data()) != LUA_OK)
            {
                throw_error(state);
            }
        }

        Var operator[](std::string_view name)
        {
            lua_getglobal(state, name.data());
            return Var(std::make_shared<Ref>(state));
        }

        template <typename T>
        void set(std::string_view name, T fn)
        {
            TypeDef<T>::push(state, std::move(fn));
            lua_setglobal(state, name.data());
        }

        template <typename C, typename MemFun>
            requires(std::is_same_v<typename nil::xalt::fn_sign<MemFun>::class_type, C>)
        void set(std::string_view name, MemFun fun, C* c)
        {
            auto fn = wrap<typename nil::xalt::fn_sign<MemFun>::return_type>(
                fun,
                c,
                typename nil::xalt::fn_sign<MemFun>::arg_types()
            );
            TypeDef<decltype(fn)>::push(state, std::move(fn));
            lua_setglobal(state, name.data());
        }

        void gc()
        {
            lua_gc(state, LUA_GCCOLLECT, 0);
        }

        int stack_depth()
        {
            return lua_gettop(state);
        }

    private:
        lua_State* state;

        template <typename R, typename C, typename MemFun, typename... A>
        static auto wrap(MemFun fun, C* c, nil::xalt::tlist_types<A...> /* arg types */)
        {
            return [c, fun](A... args) { return (c->*fun)(args...); };
        }
    };
}
