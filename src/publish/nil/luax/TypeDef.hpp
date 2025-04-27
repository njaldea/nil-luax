#pragma once

#include "Ref.hpp"
#include "error.hpp"

#include <nil/xalt/checks.hpp>
#include <nil/xalt/fn_sign.hpp>
#include <nil/xalt/str_name.hpp>
#include <nil/xalt/tlist.hpp>

extern "C"
{
#include <lua.h>
}

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

/**
 * check - only used to check constructor arg type
 * pull  - for getting data from var
 * value - for getting function args and return
 * push  - setting return value for c functions
 *       - setting globals
 */

namespace nil::luax
{
    template <typename T>
    concept is_value_type                 //
        = std::is_same_v<bool, T>         //
        || std::is_integral_v<T>          //
        || std::is_floating_point_v<T>    //
        || std::is_same_v<const char*, T> //
        || std::is_same_v<std::string, T> //
        || std::is_same_v<std::string_view, T>;

    template <typename T>
    concept is_std_fn = xalt::is_of_template_v<T, std::function>;

    template <typename T>
    struct Meta;

    template <typename T>
    concept is_user_type = requires() { Meta<T>(); };

    template <typename T>
    struct TypeDef;

    template <typename T>
    struct TypeDefCommon final
    {
        static T pull(const std::shared_ptr<Ref>& ref)
        {
            static_assert(is_value_type<std::decay_t<T>>);
            auto* state = ref->push();
            auto v = TypeDef<T>::value(state, -1);
            lua_pop(state, 1);
            return v;
        }

        static decltype(auto) pull_closure(const std::shared_ptr<Ref>& ref)
        {
            auto* state = ref->push();
            if (lua_iscfunction(state, -1) == 0)
            {
                throw_error(state);
            }
            lua_getupvalue(state, -1, 1);
            auto* value = static_cast<T*>(lua_touserdata(state, -1));
            lua_pop(state, 1);
            lua_pop(state, 1);
            return *value;
        }

        static void push_closure(lua_State* state, T context)
        {
            auto* data = static_cast<T*>(lua_newuserdata(state, sizeof(T)));
            new (data) T(std::move(context));

            lua_newtable(state);
            lua_pushcfunction(state, &TypeDefCommon<T>::del);
            lua_setfield(state, -2, "__close");
            lua_setmetatable(state, -2);

            constexpr auto closure_maker //
                = []<typename... Args, std::size_t... I>(
                      xalt::tlist_types<Args...> /* arg types */,
                      std::index_sequence<I...> /* arg indices */
                  )
            {
                return [](lua_State* s)
                {
                    auto* user_data = static_cast<T*>(lua_touserdata(s, lua_upvalueindex(1)));
                    using return_type = typename xalt::fn_sign<T>::return_type;
                    if constexpr (!std::is_same_v<void, return_type>)
                    {
                        TypeDef<return_type>::push(
                            s,
                            (*user_data)(TypeDef<Args>::value(s, I + 1)...)
                        );
                        return 1;
                    }
                    else
                    {
                        (*user_data)(TypeDef<Args>::value(s, I + 1)...);
                        return 0;
                    }
                };
            };
            constexpr auto closure = closure_maker(
                typename xalt::fn_sign<T>::arg_types(),
                std::make_index_sequence<xalt::fn_sign<T>::arg_types::size>()
            );
            lua_pushcclosure(state, closure, 1);
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
            return TypeDefCommon<bool>::pull(ref);
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
            if (!check(state, index))
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
            return TypeDefCommon<T>::pull(ref);
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
            return TypeDefCommon<T>::pull(ref);
        }
    };

    template <typename T>
        requires(std::is_same_v<std::string, T> || std::is_same_v<std::string_view, T> || std::is_same_v<const char*, T>)
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
            return T(lua_tostring(state, index));
        }

        static void push(lua_State* state, const T& value)
        {
            if constexpr (std::is_same_v<const char*, T>)
            {
                lua_pushlstring(state, value);
            }
            else
            {
                lua_pushlstring(state, value.data(), value.size());
            }
        }

        static auto pull(const std::shared_ptr<Ref>& ref)
        {
            return TypeDefCommon<std::string>::pull(ref);
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
            // pulling std::function needs to use lua api since
            // i can't guarantee if the content of the upvalue is
            // an std::function. user side might store a lambda
            // but then get an std::function out of it.

            // either to store everything in terms of std::function
            // to the upvalues, or use lua api to call the method
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
                        lua_pop(state, 1);
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
        // no push since users should not push this
        static auto pull(const std::shared_ptr<Ref>& ref)
        {
            return TypeDef<std::function<R(Args...)>>::pull(ref);
        }
    };

    template <typename T>
        requires(!is_value_type<std::decay_t<T>>) && (!is_user_type<std::remove_cvref_t<T>>)
    struct TypeDef<T> final
    {
        static void push(lua_State* state, T callable)
        {
            TypeDefCommon<T>::push_closure(state, std::move(callable));
        }

        static auto& pull(const std::shared_ptr<Ref>& ref)
        {
            return TypeDefCommon<T>::pull_closure(ref);
        }
    };

    // starting here are ref types

    template <typename T>
        requires(!is_value_type<std::decay_t<T>>) && (is_user_type<std::remove_cvref_t<T>>)
    struct TypeDef<T> final
    {
        using raw_type = std::remove_cvref_t<T>;

        static bool check(lua_State* state, int index)
        {
            return luaL_testudata(state, index, xalt::str_name_type_v<raw_type>) != nullptr;
        }

        static raw_type& value(lua_State* state, int index)
        {
            auto* data = static_cast<raw_type*>(
                luaL_testudata(state, index, xalt::str_name_type_v<raw_type>)
            );
            if (data == nullptr)
            {
                throw_error(state);
            }
            return *data;
        }

        static void push(lua_State* state, T value)
        {
            static_assert(!std::is_const_v<std::remove_reference_t<T>>);
            if constexpr (std::is_reference_v<T>)
            {
                lua_pushlightuserdata(state, &value);
            }
            else
            {
                auto* data = static_cast<raw_type*>(lua_newuserdata(state, sizeof(raw_type)));
                new (data) raw_type(std::move(value));
            }
            luaL_getmetatable(state, xalt::str_name_type_v<raw_type>);
            lua_setmetatable(state, -2);
        }

        static auto& pull(const std::shared_ptr<Ref>& ref)
        {
            auto* state = ref->push();
            auto* ptr = static_cast<raw_type*>(lua_touserdata(state, -1));
            lua_pop(state, 1);
            return *ptr;
        }
    };
}
