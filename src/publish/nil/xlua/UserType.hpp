#pragma once

#include "TypeDef.hpp"

#include <nil/xalt/fn_sign.hpp>
#include <nil/xalt/literal.hpp>
#include <nil/xalt/str_name.hpp>
#include <nil/xalt/tlist.hpp>

extern "C"
{
#include <lauxlib.h>
#include <lua.h>
}

#include <utility>

#include <iostream>

namespace nil::xlua
{
    template <typename... Args>
    struct Constructor
    {
    };

    template <xalt::literal name, auto ptr_to_member>
    struct Property
    {
    };

    template <xalt::literal name, auto ptr_to_member>
    struct Method
    {
    };

    template <typename... Types>
    struct List
    {
    };

    template <typename T>
    struct UserType
    {
    public:
        static int type_constructors(lua_State* s)
        {
            type_constructor(s, typename Type<T>::Constructors());
            return 1;
        }

        static int type_close(lua_State* s)
        {
            static_cast<T*>(lua_touserdata(s, 1))->~T();
            return 0;
        }

        static int type_index(lua_State* s)
        {
            T* data = static_cast<T*>(luaL_testudata(s, 1, xalt::str_name_type_v<T>));
            if (data == nullptr)
            {
                luaL_error(s, "[%s] is of different type", xalt::str_name_type_v<T>);
            }
            const char* key = luaL_checkstring(s, 2);
            type_get_members(data, key, hash_fnv1a(key), s, typename Type<T>::Members());
            return 1;
        }

        static int type_newindex(lua_State* s)
        {
            T* data = static_cast<T*>(luaL_testudata(s, 1, xalt::str_name_type_v<T>));
            if (data == nullptr)
            {
                luaL_error(s, "[%s] is of different type", xalt::str_name_type_v<T>);
            }
            const char* key = luaL_checkstring(s, 2);
            type_set_members(data, key, hash_fnv1a(key), s, typename Type<T>::Members());
            return 1;
        }

        static int type_call(lua_State* s)
        {
            return type_method_call<&T::operator()>(s);
        }

        static int type_pairs(lua_State* s)
        {
            lua_pushcfunction(
                s,
                [](lua_State* ss)
                {
                    T* data = static_cast<T*>(luaL_testudata(ss, 1, xalt::str_name_type_v<T>));
                    if (lua_isnil(ss, 2))
                    {
                        return type_pairs_iter(data, ss, nullptr, typename Type<T>::Members());
                    }
                    else
                    {
                        const auto hash = hash_fnv1a(lua_tostring(ss, 2));
                        return type_pairs_iter(data, ss, &hash, typename Type<T>::Members());
                    }
                }
            );
            lua_pushlightuserdata(s, luaL_testudata(s, 1, xalt::str_name_type_v<T>));
            luaL_getmetatable(s, xalt::str_name_type_v<T>);
            lua_setmetatable(s, -2);
            lua_pushnil(s);
            return 3;
        }

    private:
        template <xalt::literal l, auto p, typename... Rest>
        static int
            type_pairs_iter(T* data, lua_State* state, const std::uint32_t* key_hash, List<Property<l, p>, Rest...>)
        {
            if (key_hash == nullptr)
            {
                lua_pushlstring(state, xalt::literal_v<l>, sizeof(l) - 1);
                TypeDef<decltype(data->*p)>::push(state, data->*p);
                return 2;
            }
            else
            {
                static constinit auto hash = hash_fnv1a(Property<l, p>());
                if (*key_hash == hash)
                {
                    return type_pairs_iter(data, state, nullptr, List<Rest...>());
                }
                else
                {
                    return type_pairs_iter(data, state, key_hash, List<Rest...>());
                }
            }
        }

        template <xalt::literal l, auto p, typename... Rest>
        static int
            type_pairs_iter(T* data, lua_State* s, const std::uint32_t* key_hash, List<Method<l, p>, Rest...>)
        {
            return type_pairs_iter(data, s, key_hash, List<Rest...>());
        }

        static int type_pairs_iter(
            T* /* data */,
            lua_State* state,
            const std::uint32_t* /* key_hash */,
            List<> /* members */
        )
        {
            lua_pushnil(state);
            return 1;
        }

        template <typename... CType, typename... TRest>
        static void type_constructor(
            lua_State* s,
            List<Constructor<CType...>, TRest...> /* constructors */
        )
        {
            constexpr auto match = []<std::size_t... I>(lua_State* ss, std::index_sequence<I...>)
            {
                if (sizeof...(CType) == lua_gettop(ss)
                    && (true && ... && TypeDef<CType>::check(ss, I + 1)))
                {
                    T* data = static_cast<T*>(lua_newuserdata(ss, sizeof(T)));
                    new (data) T(TypeDef<CType>::value(ss, I + 1)...);
                    luaL_getmetatable(ss, xalt::str_name_type_v<T>);
                    lua_setmetatable(ss, -2);
                    return false;
                }
                return true;
            };

            if (match(s, std::make_index_sequence<sizeof...(CType)>()))
            {
                if constexpr (sizeof...(TRest) == 0)
                {
                    luaL_error(s, "[%s] can't be constructed with the provided arguments", xalt::str_name_type_v<T>);
                }
                else
                {
                    type_constructor(s, List<TRest...>());
                }
            }
        }

        template <auto member>
        static int type_method_call(lua_State* s)
        {
            using fn_sign = xalt::fn_sign<decltype(member)>;
            using Args = typename fn_sign::arg_types;

            constexpr auto fn          //
                = []<typename... Args, //
                     std::size_t... I> //
                (lua_State * ss, xalt::tlist_types<Args...>, std::index_sequence<I...>)
            {
                T* data = static_cast<T*>(lua_touserdata(ss, 1));
                using R = typename fn_sign::return_type;
                if constexpr (!std::is_same_v<void, R>)
                {
                    TypeDef<R>::push((data->*member)(TypeDef<Args>::value(ss, I + 2)...));
                    return 1;
                }
                else
                {
                    (data->*member)(TypeDef<Args>::value(ss, I + 2)...);
                    return 0;
                }
            };

            return fn(s, Args(), std::make_index_sequence<Args::size>());
        }

        template <xalt::literal l, auto p>
        static void type_get_member(T* /* data */, Method<l, p> /* member */, lua_State* s)
        {
            lua_pushcfunction(s, &type_method_call<p>);
        }

        template <xalt::literal l, auto p>
        static void type_get_member(T* data, Property<l, p> /* member */, lua_State* s)
        {
            TypeDef<decltype(data->*p)>::push(s, data->*p);
        }

        template <typename M, typename... TRest>
        static void type_get_members(
            T* data,
            const char* key,
            std::uint32_t key_hash,
            lua_State* s,
            List<M, TRest...> /* props */
        )
        {
            static constinit auto hash = hash_fnv1a(M());
            if (key_hash == hash)
            {
                type_get_member(data, M(), s);
                return;
            }
            if constexpr (sizeof...(TRest) > 0)
            {
                type_get_members(data, key, key_hash, s, List<TRest...>());
            }
            else
            {
                luaL_error(s, "[%s] member [%s] is unknown", xalt::str_name_type_v<T>, key);
            }
        }

        template <xalt::literal l, auto p>
        static void type_set_member(T* /* data */, Method<l, p> /* member */, lua_State* s)
        {
            luaL_error(s, "[%s] member functions should not be replaced", xalt::str_name_type_v<T>);
        }

        template <xalt::literal l, auto p>
        static void type_set_member(T* data, Property<l, p> /* member */, lua_State* s)
        {
            data->*p = TypeDef<decltype(data->*p)>::value(s, 3);
        }

        template <typename M, typename... TRest>
        static void type_set_members(
            T* data,
            const char* key,
            std::uint32_t key_hash,
            lua_State* s,
            List<M, TRest...> /* props */
        )
        {
            static constinit auto hash = hash_fnv1a(M());
            if (key_hash == hash)
            {
                type_set_member(data, M(), s);
                return;
            }
            if constexpr (sizeof...(TRest) > 0)
            {
                type_set_members(data, key, key_hash, s, List<TRest...>());
            }
            else
            {
                luaL_error(s, "[%s] member [%s] is unknown", xalt::str_name_type_v<T>, key);
            }
        }

        template <xalt::literal l, auto r>
        static consteval auto hash_fnv1a(Property<l, r> /* info */) -> std::uint32_t
        {
            return hash_fnv1a(xalt::literal_v<l>);
        }

        template <xalt::literal l, auto r>
        static consteval auto hash_fnv1a(Method<l, r> /* info */) -> std::uint32_t
        {
            return hash_fnv1a(xalt::literal_v<l>);
        }

        static constexpr auto hash_fnv1a(
            std::string_view literal,                 // NOLINT
            std::uint32_t offset_basis = 2166136261u, // NOLINT
            std::uint32_t prime = 16777619u           // NOLINT
        ) -> std::uint32_t
        {
            std::uint32_t hash = offset_basis;
            for (auto i : literal)
            {
                hash ^= static_cast<std::uint8_t>(i);
                hash *= prime;
            }
            return hash;
        }
    };
}
