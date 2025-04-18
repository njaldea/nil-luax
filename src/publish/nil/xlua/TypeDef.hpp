#pragma once

#include "Ref.hpp"
#include "error.hpp"

#include <nil/xalt/fn_sign.hpp>
#include <nil/xalt/str_name.hpp>
#include <nil/xalt/tlist.hpp>

extern "C"
{
#include "lua.h"
}

#include <functional>
#include <memory>

namespace nil::xlua
{
    template <typename T>
    struct TypeDef;

    template <typename T>
    struct TypeDefCommon final
    {
        static auto pull(const std::shared_ptr<Ref>& ref)
        {
            auto* state = ref->push();
            auto v = TypeDef<T>::value(state, -1);
            lua_pop(state, -1);
            return v;
        }

        static int del(lua_State* state)
        {
            static_cast<T*>(lua_touserdata(state, 1))->~T();
            return 0;
        }
    };

    template <>
    struct TypeDef<bool> final
    {
        using type = bool;

        static bool value(lua_State* state, int index)
        {
            if (!lua_isboolean(state, index))
            {
                throw_error(state);
            }
            return lua_toboolean(state, index) > 0;
        }

        static void push(lua_State* state, bool value)
        {
            lua_pushboolean(state, value ? 1 : 0);
        }

        static type pull(const std::shared_ptr<Ref>& ref)
        {
            return TypeDefCommon<bool>::pull(ref);
        }
    };

    template <typename T>
        requires(std::is_floating_point_v<T>)
    struct TypeDef<T> final
    {
        using type = T;

        static type value(lua_State* state, int index)
        {
            if (lua_isnumber(state, index) == 0)
            {
                throw_error(state);
            }
            return type(lua_tonumber(state, index));
        }

        static void push(lua_State* state, type value)
        {
            lua_pushnumber(state, value);
        }

        static type pull(const std::shared_ptr<Ref>& ref)
        {
            return TypeDefCommon<T>::pull(ref);
        }
    };

    template <typename T>
        requires(std::is_integral_v<T>)
    struct TypeDef<T> final
    {
        using type = T;

        static type value(lua_State* state, int index)
        {
            if (lua_isinteger(state, index) == 0)
            {
                throw_error(state);
            }
            return type(lua_tointeger(state, index));
        }

        static void push(lua_State* state, type value)
        {
            lua_pushinteger(state, value);
        }

        static type pull(const std::shared_ptr<Ref>& ref)
        {
            return TypeDefCommon<T>::pull(ref);
        }
    };

    template <typename T>
        requires(std::is_same_v<std::string, T> || std::is_same_v<std::string_view, T>)
    struct TypeDef<T> final
    {
        using type = T;

        static auto value(lua_State* state, int index)
        {
            if (lua_isstring(state, index) == 0)
            {
                throw_error(state);
            }
            return std::string(lua_tostring(state, index));
        }

        static void push(lua_State* state, const type& value)
        {
            lua_pushlstring(state, value.data(), value.size());
        }

        static T pull(const std::shared_ptr<Ref>& ref)
        {
            return TypeDefCommon<T>::pull(ref);
        }
    };

    template <typename R, typename... Args>
    struct TypeDef<R(Args...)> final
    {
        using type = std::function<R(Args...)>;

        static type pull(const std::shared_ptr<Ref>& ref)
        {
            return [ref](Args... args)
            {
                auto* state = ref->push();
                if (!lua_isfunction(state, -1))
                {
                    throw_error(state);
                }

                (TypeDef<Args>::push(state, args), ...);

                constexpr auto return_size = std::is_same_v<R, void> ? 0 : 1;
                if (lua_pcall(state, sizeof...(Args), return_size, 0) != LUA_OK)
                {
                    throw_error(state);
                }

                if constexpr (!std::is_same_v<void, R>)
                {
                    auto value = TypeDef<R>::value(state, -1);
                    lua_pop(state, -1);
                    return value;
                }
            };
        }
    };

    template <typename T>
        requires requires() { &T::operator(); }
    struct TypeDef<T> final
    {
    private:
        using fn_sign = nil::xalt::fn_sign<T>;
        using return_type = fn_sign::return_type;
        using arg_types = fn_sign::arg_types;

    public:
        using type = T;

        static void push(lua_State* state, T callable)
        {
            auto* data = static_cast<T*>(lua_newuserdata(state, sizeof(T)));
            new (data) T(std::move(callable));

            lua_newtable(state);
            lua_pushcfunction(state, &TypeDefCommon<T>::del);
            lua_setfield(state, -2, "__gc");
            lua_setmetatable(state, -2);

            lua_pushcclosure(
                state,
                [](lua_State* s) -> int
                {
                    auto* user_data = static_cast<T*>(lua_touserdata(s, lua_upvalueindex(1)));
                    constexpr auto arg_indices = std::make_index_sequence<arg_types::size>();
                    return call(s, user_data, arg_types(), arg_indices);
                },
                1
            );
        }

        static type pull(const std::shared_ptr<Ref>& ref)
        {
            using fn_ptr_type = decltype(fn(std::declval<typename fn_sign::arg_types>()));
            using fn_type = std::remove_pointer_t<fn_ptr_type>;
            return TypeDef<fn_type>::pull(ref);
        }

    private:
        template <typename... Args, std::size_t... I>
        static int call(
            lua_State* state,
            T* user_data,
            nil::xalt::tlist_types<Args...> /* types */,
            std::index_sequence<I...> /* indices */
        )
        {
            if constexpr (!std::is_same_v<void, return_type>)
            {
                TypeDef<return_type>::push(
                    state,
                    (*user_data)(TypeDef<Args>::value(state, I + 1)...)
                );
                return 1;
            }
            else
            {
                (*user_data)(TypeDef<Args>::value(state, I + 1)...);
                return 0;
            }
        }

        template <typename... Args>
        static auto fn(nil::xalt::tlist_types<Args...> /* types */) ->
            typename nil::xalt::fn_sign<T>::return_type (*)(Args...);
    };
}
