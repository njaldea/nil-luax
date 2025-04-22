#pragma once

#include "TypeDef.hpp"

#include <lua.h>
#include <nil/xalt/fn_sign.hpp>
#include <nil/xalt/literal.hpp>
#include <nil/xalt/str_name.hpp>
#include <nil/xalt/tlist.hpp>

extern "C"
{
#include <lauxlib.h>
}

#include <utility>

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
    struct Type;

    template <typename T>
    struct UserType
    {
    public:
        static int type_constructors(lua_State* s)
        {
            if constexpr (requires() { typename Type<T>::Constructors; })
            {
                type_constructor(s, typename Type<T>::Constructors());
                return 1;
            }
            else
            {
                return 0;
            }
        }

        static int type_close(lua_State* s)
        {
            static_cast<T*>(lua_touserdata(s, 1))->~T();
            return 0;
        }

        static int type_index(lua_State* s)
        {
            if constexpr (requires() { typename Type<T>::Members; })
            {
                T* data = static_cast<T*>(luaL_checkudata(s, 1, xalt::str_name_type_v<T>.data()));
                const char* key = luaL_checkstring(s, 2);
                type_get_members(data, key, hash_fnv1a(key), s, typename Type<T>::Members());
                return 1;
            }
            else
            {
                return 0;
            }
        }

        static int type_newindex(lua_State* s)
        {
            if constexpr (requires() { typename Type<T>::Members; })
            {
                T* data = static_cast<T*>(luaL_checkudata(s, 1, xalt::str_name_type_v<T>.data()));
                const char* key = luaL_checkstring(s, 2);
                type_set_members(data, key, hash_fnv1a(key), s, typename Type<T>::Members());
                return 1;
            }
            else
            {
                return 0;
            }
        }

    private:
        template <typename... CType, typename... TRest>
        static void type_constructor(
            lua_State* s,
            List<Constructor<CType...>, TRest...> /* constructors */
        )
        {
            if ([s]<std::size_t... I>(std::index_sequence<I...>)
            {
                if (sizeof...(CType) != lua_gettop(s)) {
                    return false;
                }
                if ((true && ... && TypeDef<CType>::check(s, I + 1)))
                {
                    auto* data = static_cast<T*>(lua_newuserdata(s, sizeof(T)));
                    new (data) T(TypeDef<CType>::value(s, I + 1)...);
                    luaL_getmetatable(s, xalt::str_name_type_v<T>.data());
                    lua_setmetatable(s, -2);
                    return true;
                }
                return false;
            }(std::make_index_sequence<sizeof...(CType)>()))
            {
                if constexpr (sizeof...(TRest) == 0)
                {
                    static constexpr const auto* meta = xalt::str_name_type_v<T>.data();
                    luaL_error(s, "[%s] can't be constructed with the provided arguments", meta);
                }
                else
                {
                    type_constructor(s, List<TRest...>());
                }
            }
        }

        template <xalt::literal l, auto p>
        static void type_get_member(T* /* data */, Method<l, p> /* member */, lua_State* s)
        {
            constexpr auto mm = []<typename... Args>(auto pp, xalt::tlist_types<Args...>)
            { return [pp](T& d, Args... args) { return (d.*pp)(args...); }; };
            auto method = mm(p, typename xalt::fn_sign<decltype(p)>::arg_types());
            TypeDef<decltype(method)>::push(s, std::move(method));
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
                static constexpr const auto* meta = xalt::str_name_type_v<T>.data();
                luaL_error(s, "[%s] member [%s] is unknown", meta, key);
            }
        }

        template <xalt::literal l, auto p>
        static void type_set_member(T* /* data */, Method<l, p> /* member */, lua_State* s)
        {
            static constexpr const auto* meta = xalt::str_name_type_v<T>.data();
            luaL_error(s, "[%s] member functions should not be replaced", meta);
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
            static constexpr auto hash = hash_fnv1a(M());
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
                static constexpr const auto* meta = xalt::str_name_type_v<T>.data();
                luaL_error(s, "[%s] member [%s] is unknown", meta, key);
            }
        }

        template <xalt::literal l, auto r>
        static constexpr auto hash_fnv1a(Property<l, r> /* info */) -> std::uint32_t
        {
            return hash_fnv1a(xalt::literal_v<l>);
        }

        template <xalt::literal l, auto r>
        static constexpr auto hash_fnv1a(Method<l, r> /* info */) -> std::uint32_t
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
