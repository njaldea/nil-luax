#pragma once

#include "Ref.hpp"
#include "UserType.hpp"
#include "Var.hpp"

#include <nil/xalt/fn_sign.hpp>
#include <nil/xalt/str_name.hpp>
#include <nil/xalt/tlist.hpp>

extern "C"
{
#include <lauxlib.h>
#include <lualib.h>
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
            return this->get(name);
        }

        Var get(std::string_view name)
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
            requires(std::is_pointer_v<MemFun> && std::is_same_v<typename xalt::fn_sign<MemFun>::class_type, C>)
        void set(std::string_view name, MemFun fun, C* context)
        {
            constexpr auto wrap //
                = []<typename... Args>(C* c, MemFun f, xalt::tlist_types<Args...>)
            { return [c, f](Args... args) { return (c->*f)(args...); }; };
            set(name, wrap(context, fun, typename xalt::fn_sign<MemFun>::arg_types()));
        }

        template <typename FreeFun>
            requires(std::is_pointer_v<FreeFun> && std::is_same_v<typename xalt::fn_sign<FreeFun>::class_type, void>)
        void set(std::string_view name, FreeFun fun)
        {
            constexpr auto wrap //
                = []<typename... Args>(FreeFun f, xalt::tlist_types<Args...>)
            { return [f](Args... args) { return f(args...); }; };
            set(name, wrap(fun, typename xalt::fn_sign<FreeFun>::arg_types()));
        }

        void gc()
        {
            lua_gc(state, LUA_GCCOLLECT, 0);
        }

        int stack_depth()
        {
            return lua_gettop(state);
        }

        template <typename T>
        void add_type(std::string_view name)
        {
            if constexpr (requires() { typename Type<T>::Constructors; })
            {
                lua_register(state, name.data(), &UserType<T>::type_constructors);
            }
            luaL_newmetatable(state, xalt::str_name_type_v<T>);
            lua_pushcfunction(state, &UserType<T>::type_close);
            lua_setfield(state, -2, "__close");
            lua_pushcfunction(state, &UserType<T>::type_index);
            lua_setfield(state, -2, "__index");
            lua_pushcfunction(state, &UserType<T>::type_newindex);
            lua_setfield(state, -2, "__newindex");
            if constexpr (requires() { &T::operator(); })
            {
                lua_pushcfunction(state, &UserType<T>::type_call);
                lua_setfield(state, -2, "__call");
            }
            // TODO:
            //  -  __tostring
            //  -  __concat
            //  -  __gc (?)
            //  -  __pairs
            //  -  __call
        }

    private:
        lua_State* state;
    };
}
