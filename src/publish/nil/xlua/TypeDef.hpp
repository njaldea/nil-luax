#pragma once

#include "Ref.hpp"
#include "error.hpp"

#include <nil/xalt/fn_sign.hpp>
#include <nil/xalt/str_name.hpp>
#include <nil/xalt/tlist.hpp>
#include <type_traits>
#include <utility>

extern "C"
{
#include "lua.h"
}

#include <functional>
#include <memory>

/**
 * check - only used to check constructor arg type
 * pull  - for getting data from var
 * value - for getting function args and return
 * push  - setting return value for c functions
 *       - setting globals
 */

namespace nil::xlua
{
    template <typename T>
    concept is_value_type                 //
        = std::is_same_v<bool, T>         //
        || std::is_integral_v<T>          //
        || std::is_floating_point_v<T>    //
        || std::is_same_v<std::string, T> //
        || std::is_same_v<std::string_view, T>;

    template <typename T>
    concept is_function_type //
        = requires() { &T::operator(); };

    template <typename T>
    struct TypeDef;

    template <typename T>
    struct TypeDefCommon final
    {
        static auto pull_value(const std::shared_ptr<Ref>& ref)
        {
            auto* state = ref->push();
            auto v = TypeDef<T>::value(state, -1);
            lua_pop(state, -1);
            return v;
        }

        static auto& pull_ref(const std::shared_ptr<Ref>& ref)
        {
            auto* state = ref->push();
            auto* ptr = static_cast<T*>(lua_touserdata(state, -1));
            lua_pop(state, -1);
            return *ptr;
        }

        static auto push_closure(lua_State* state, T context)
        {
            auto* data = static_cast<T*>(lua_newuserdata(state, sizeof(T)));
            new (data) T(std::move(context));

            lua_newtable(state);
            lua_pushcfunction(state, &TypeDefCommon<T>::del);
            lua_setfield(state, -2, "__gc");
            lua_setmetatable(state, -2);

            using fn_sign = nil::xalt::fn_sign<T>;
            lua_pushcclosure(
                state,
                []<typename... Args,
                   std::size_t... I>(nil::xalt::tlist_types<Args...>, std::index_sequence<I...>)
                {
                    return [](lua_State* s)
                    {
                        auto* user_data = static_cast<T*>(lua_touserdata(s, lua_upvalueindex(1)));
                        using return_type = typename fn_sign::return_type;
                        if constexpr (!std::is_same_v<void, return_type>)
                        {
                            TypeDef<return_type>::push(
                                s,
                                (*user_data)(TypeDef<std::remove_cvref_t<Args>>::value(s, I + 1)...)
                            );
                            return 1;
                        }
                        else
                        {
                            (*user_data)(TypeDef<std::remove_cvref_t<Args>>::value(s, I + 1)...);
                            return 0;
                        }
                    };
                }(fn_sign::arg_types(), std::make_index_sequence<fn_sign::arg_types::size>())
            );
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
        static bool check(lua_State* state, int index)
        {
            return lua_isboolean(state, index);
        }

        static bool value(lua_State* state, int index)
        {
            if (!check(state, index))
            {
                throw_error(state);
            }
            return lua_toboolean(state, index) > 0;
        }

        static void push(lua_State* state, bool value)
        {
            lua_pushboolean(state, value ? 1 : 0);
        }

        static auto pull(const std::shared_ptr<Ref>& ref)
        {
            return TypeDefCommon<bool>::pull_value(ref);
        }
    };

    template <typename T>
        requires(std::is_floating_point_v<T>)
    struct TypeDef<T> final
    {
        static bool check(lua_State* state, int index)
        {
            return lua_isnumber(state, index) != 0;
        }

        static T value(lua_State* state, int index)
        {
            if (check(state, index))
            {
                throw_error(state);
            }
            return T(lua_tonumber(state, index));
        }

        static void push(lua_State* state, T value)
        {
            lua_pushnumber(state, value);
        }

        static auto pull(const std::shared_ptr<Ref>& ref)
        {
            return TypeDefCommon<T>::pull_value(ref);
        }
    };

    template <typename T>
        requires(std::is_integral_v<T>)
    struct TypeDef<T> final
    {
        static bool check(lua_State* state, int index)
        {
            return lua_isinteger(state, index) != 0;
        }

        static T value(lua_State* state, int index)
        {
            if (!check(state, index))
            {
                throw_error(state);
            }
            return T(lua_tointeger(state, index));
        }

        static void push(lua_State* state, T value)
        {
            lua_pushinteger(state, value);
        }

        static auto pull(const std::shared_ptr<Ref>& ref)
        {
            return TypeDefCommon<T>::pull_value(ref);
        }
    };

    template <typename T>
        requires(std::is_same_v<std::string, T> || std::is_same_v<std::string_view, T>)
    struct TypeDef<T> final
    {
        static bool check(lua_State* state, int index)
        {
            return lua_isstring(state, index) != 0;
        }

        static auto value(lua_State* state, int index)
        {
            if (!check(state, index))
            {
                throw_error(state);
            }
            return std::string(lua_tostring(state, index));
        }

        static void push(lua_State* state, const T& value)
        {
            lua_pushlstring(state, value.data(), value.size());
        }

        static auto pull(const std::shared_ptr<Ref>& ref)
        {
            return TypeDefCommon<T>::pull_value(ref);
        }
    };

    template <typename T>
        requires(is_value_type<std::decay_t<T>>)
    struct TypeDef<T&> final
    {
        using raw_type = std::remove_cvref_t<T>;

        static bool check(lua_State* state, int index)
        {
            return TypeDef<raw_type>::check(state, index);
        }

        static decltype(auto) value(lua_State* state, int index)
        {
            return TypeDef<raw_type>::value(state, index);
        }

        static void push(lua_State* state, const raw_type& value)
        {
            TypeDef<raw_type>::push(state, value);
        }

        static decltype(auto) pull(const std::shared_ptr<Ref>& ref)
        {
            return TypeDef<raw_type>::pull(ref);
        }
    };

    // starting here are callable

    template <typename R, typename... Args>
    struct TypeDef<std::function<R(Args...)>> final
    {
        static bool check(lua_State* state, int index)
        {
            return lua_isfunction(state, index);
        }

        static std::function<R(Args...)> value(lua_State* state, int index)
        {
            if (!check(state, index))
            {
                throw_error(state);
            }
            return lua_toboolean(state, index) > 0;
        }

        static auto pull(const std::shared_ptr<Ref>& ref)
        {
            return std::function<R(Args...)>(
                [ref](Args... args)
                {
                    auto* state = ref->push();
                    if (!lua_isfunction(state, -1))
                    {
                        throw_error(state);
                    }

                    (TypeDef<Args>::push(state, static_cast<Args>(args)), ...);

                    constexpr auto return_size = std::is_same_v<R, void> ? 0 : 1;
                    if (lua_pcall(state, sizeof...(Args), return_size, 0) != LUA_OK)
                    {
                        throw_error(state);
                    }

                    static_assert(!std::is_reference_v<R>);
                    if constexpr (!std::is_same_v<void, R> && !std::is_reference_v<R>)
                    {
                        auto value = TypeDef<R>::value(state, -1);
                        lua_pop(state, -1);
                        return value;
                    }
                }
            );
        }

        static void push(lua_State* state, std::function<R(Args...)> callable)
        {
            TypeDefCommon<std::function<R(Args...)>>::push_closure(state, std::move(callable));
        }
    };

    // this is only usable for Var
    // not argument/return of a function
    template <typename R, typename... Args>
    struct TypeDef<R(Args...)> final
    {
        static_assert(!std::is_reference_v<R>, "return type can't be reference");

        // no push since users should not push this
        static auto pull(const std::shared_ptr<Ref>& ref)
        {
            return TypeDef<std::function<R(Args...)>>::pull(ref);
        }
    };

    template <is_function_type T>
        requires(!std::is_reference_v<typename nil::xalt::fn_sign<T>::return_type>)
    struct TypeDef<T> final
    {
        static void push(lua_State* state, T callable)
        {
            TypeDefCommon<T>::push_closure(state, std::move(callable));
        }

        static auto& pull(const std::shared_ptr<Ref>& ref)
        {
            return TypeDefCommon<T>::pull_ref(ref);
        }
    };

    template <typename R, typename... Args>
    struct TypeDef<R&(Args...)> final
    {
        static_assert(!std::is_reference_v<R&>, "return type can't be reference");
    };

    template <typename R, typename... Args>
    struct TypeDef<std::function<R&(Args...)>> final
    {
        static_assert(!std::is_reference_v<R&>, "return type can't be reference");
    };

    template <is_function_type T>
        requires(std::is_reference_v<typename nil::xalt::fn_sign<T>::return_type>)
    struct TypeDef<T> final
    {
        static_assert(
            !std::is_reference_v<typename nil::xalt::fn_sign<T>::return_type>,
            "return type can't be reference"
        );
    };

    // starting here are ref types

    template <typename T>
        requires(!is_value_type<std::decay_t<T>>)
    struct TypeDef<T> final
    {
        using raw_type = std::remove_cvref_t<T>;

        static bool check(lua_State* state, int index)
        {
            return luaL_checkudata(state, index, nil::xalt::str_name_type_v<raw_type>);
        }

        static T value(lua_State* state, int index)
        {
            if (!check(state, index))
            {
                throw_error(state);
            }
            return *static_cast<raw_type*>(lua_touserdata(state, index));
        }

        static void push(lua_State* state, T callable)
        {
            if constexpr (std::is_reference_v<T>)
            {
                lua_pushlightuserdata(state, &callable);
                luaL_getmetatable(state, nil::xalt::str_name_type_v<raw_type>);
                lua_setmetatable(state, -2);
            }
            else
            {
                auto* data = static_cast<raw_type*>(lua_newuserdata(state, sizeof(raw_type)));
                new (data) raw_type(std::move(callable));
            }
        }

        static auto& pull(const std::shared_ptr<Ref>& ref)
        {
            return TypeDefCommon<raw_type>::pull_ref(ref);
        }
    };
}
