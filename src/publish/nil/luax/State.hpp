#pragma once

#include "Ref.hpp"
#include "TypeDef.hpp"
#include "UserType.hpp"
#include "Var.hpp"

#include <nil/xalt/fn_sign.hpp>
#include <nil/xalt/str_name.hpp>
#include <nil/xalt/tlist.hpp>

extern "C"
{
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <string_view>
#include <type_traits>

namespace nil::luax
{
    class State final
    {
    private:
        template <typename T>
        consteval static bool is_valid_set()
        {
            using raw_type = std::remove_cvref_t<T>;
            if constexpr (is_user_type<raw_type>)
            {
                return (!std::is_rvalue_reference_v<T> || !std::is_const_v<std::remove_reference_t<T>>);
            }
            else
            {
                return std::is_same_v<raw_type, T>;
            }
        }

    public:
        State() = default;

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

        void open_libs()
        {
            luaL_openlibs(state);
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

        Var get(std::string_view name)
        {
            lua_getglobal(state, name.data());
            return Var(std::make_shared<Ref>(state));
        }

        template <typename T>
            requires(!is_valid_set<T>())
        void set(std::string_view name, T&& fn) = delete;

        template <typename T>
            requires(is_valid_set<T>())
        void set(std::string_view name, T&& fn)
        {
            TypeDef<T>::push(state, std::forward<T>(fn));
            lua_setglobal(state, name.data());
        }

        template <typename C, typename Return, typename... Args>
        void set(std::string_view name, Return (C::*fn)(Args...), C* context)
        {
            set(name,
                [context, fn](Args... args)
                { return (context->*fn)(std::forward<Args>(args)...); });
        }

        template <typename Return, typename... Args>
        void set(std::string_view name, Return (*fn)(Args...))
        {
            set(name, [fn](Args... args) { return fn(std::forward<Args>(args)...); });
        }

        void gc()
        {
            lua_gc(state, LUA_GCCOLLECT, 0);
        }

        int stack_depth()
        {
            return lua_gettop(state);
        }

        template <is_user_type T>
        void add_type()
        {
            luaL_newmetatable(state, xalt::str_name_type_v<T>);
            lua_pushcfunction(state, &UserType<T>::type_close);
            lua_setfield(state, -2, "__close");
            if constexpr (requires() { &T::operator(); })
            {
                lua_pushcfunction(state, &UserType<T>::type_call);
                lua_setfield(state, -2, "__call");
            }
            if constexpr (requires() { typename Meta<T>::Members; })
            {
                lua_pushcfunction(state, &UserType<T>::type_index);
                lua_setfield(state, -2, "__index");
                lua_pushcfunction(state, &UserType<T>::type_newindex);
                lua_setfield(state, -2, "__newindex");
                lua_pushcfunction(state, &UserType<T>::type_pairs);
                lua_setfield(state, -2, "__pairs");
            }
            // TODO:
            //  -  __tostring
            //  -  __concat
            //  -  __gc (?)
        }

        template <is_user_type T>
            requires requires() { typename Meta<T>::Constructors; }
        void add_type(std::string_view name)
        {
            lua_register(state, name.data(), &UserType<T>::type_constructors);
            add_type<T>();
        }

    private:
        lua_State* state = luaL_newstate();
    };
}
