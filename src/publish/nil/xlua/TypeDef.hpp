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

#include <memory>
#include <optional>

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
            return TypeDefCommon<T>::pull(ref);
        }
    };

    template <typename T>
    struct TypeDef<std::optional<T>> final
    {
        static bool check(lua_State* state, int index)
        {
            return (lua_isnil(state, index) != 0) || TypeDef<T>::check(state, index);
        }

        static auto value(lua_State* state, int index)
        {
            if (lua_isnil(state, index) != 0)
            {
                return std::optional<T>();
            }
            return std::make_optional(TypeDef<T>::value(state, index));
        }

        static void push(lua_State* state, const std::optional<T>& value)
        {
            if (value.has_value())
            {
                TypeDef<T>::push(state, value.value());
            }
            lua_pushnil(state);
        }

        static auto pull(const std::shared_ptr<Ref>& ref)
        {
            auto* state = ref->push();
            if (lua_isnil(state, -1))
            {
                return std::optional<T>();
            }
            return std::make_optional(TypeDef<T>::pull(ref));
        }
    };

    template <typename R, typename... Args>
    struct TypeDef<R(Args...)> final
    {
        static auto pull(const std::shared_ptr<Ref>& ref)
        {
            return [ref](Args... args)
            {
                auto* state = ref->push();
                if (!lua_isfunction(state, -1))
                {
                    throw_error(state);
                }

                (TypeDef<std::remove_cvref_t<Args>>::push(state, std::move(args)), ...);

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
        static void push(lua_State* state, T callable)
        {
            auto* data = static_cast<T*>(lua_newuserdata(state, sizeof(T)));
            new (data) T(std::move(callable));

            lua_newtable(state);
            lua_pushcfunction(state, &TypeDefCommon<T>::del);
            lua_setfield(state, -2, "__gc");
            lua_setmetatable(state, -2);

            using arg_types = typename nil::xalt::fn_sign<T>::arg_types;
            lua_pushcclosure(
                state,
                []<typename... Args, std::size_t... I>(
                    nil::xalt::tlist_types<Args...> /* types */,
                    std::index_sequence<I...> /* indices */
                )
                {
                    return [](lua_State* s) -> int
                    {
                        auto* user_data = static_cast<T*>(lua_touserdata(s, lua_upvalueindex(1)));
                        using return_type = typename nil::xalt::fn_sign<T>::return_type;
                        using type_def = TypeDef<return_type>;
                        if constexpr (!std::is_same_v<void, return_type>)
                        {
                            type_def::push(
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
                }(arg_types(), std::make_index_sequence<arg_types::size>()),
                1
            );
        }

        static auto pull(const std::shared_ptr<Ref>& ref)
        {
            using free_type = typename nil::xalt::fn_sign<T>::free_type;
            return TypeDef<free_type>::pull(ref);
        }
    };

    template <typename T>
    struct TypeDef
    {
        static void push(lua_State* state, T callable)
        {
            auto* data = static_cast<T*>(lua_newuserdata(state, sizeof(T)));
            new (data) T(std::move(callable));

            lua_newtable(state);
            lua_pushcfunction(state, &TypeDefCommon<T>::del);
            lua_setfield(state, -2, "__gc");
            lua_setmetatable(state, -2);

            using arg_types = typename nil::xalt::fn_sign<T>::arg_types;
            lua_pushcclosure(
                state,
                []<typename... Args, std::size_t... I>(
                    nil::xalt::tlist_types<Args...> /* types */,
                    std::index_sequence<I...> /* indices */
                )
                {
                    return [](lua_State* s) -> int
                    {
                        auto* user_data = static_cast<T*>(lua_touserdata(s, lua_upvalueindex(1)));
                        using return_type = typename nil::xalt::fn_sign<T>::return_type;
                        using type_def = TypeDef<return_type>;
                        if constexpr (!std::is_same_v<void, return_type>)
                        {
                            type_def::push(
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
                }(arg_types(), std::make_index_sequence<arg_types::size>()),
                1
            );
        }

        static auto pull(const std::shared_ptr<Ref>& ref)
        {
            using free_type = typename nil::xalt::fn_sign<T>::free_type;
            return TypeDef<free_type>::pull(ref);
        }
    };
}
