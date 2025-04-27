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

namespace nil::luax
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

    template <is_user_type T>
    struct UserType
    {
    public:
        static int type_constructors(lua_State* state)
        {
            type_constructor(state, typename Meta<T>::Constructors());
            return 1;
        }

        static int type_close(lua_State* state)
        {
            static_cast<T*>(lua_touserdata(state, 1))->~T();
            return 0;
        }

        static int type_index(lua_State* state)
        {
            T* data = static_cast<T*>(luaL_testudata(state, 1, xalt::str_name_type_v<T>));
            if (data == nullptr)
            {
                luaL_error(state, "[%s] is of different type", xalt::str_name_type_v<T>);
            }
            const char* key = luaL_checkstring(state, 2);
            type_get_members(state, typename Meta<T>::Members(), data, key, hash_fnv1a(key));
            return 1;
        }

        static int type_newindex(lua_State* state)
        {
            T* data = static_cast<T*>(luaL_testudata(state, 1, xalt::str_name_type_v<T>));
            if (data == nullptr)
            {
                luaL_error(state, "[%s] is of different type", xalt::str_name_type_v<T>);
            }
            const char* key = luaL_checkstring(state, 2);
            type_set_members(state, typename Meta<T>::Members(), data, key, hash_fnv1a(key));
            return 1;
        }

        static int type_call(lua_State* state)
        {
            return type_method_call<&T::operator()>(state);
        }

        static int type_pairs(lua_State* state)
        {
            lua_pushcfunction(
                state,
                [](lua_State* ss)
                {
                    T* data = static_cast<T*>(luaL_testudata(ss, 1, xalt::str_name_type_v<T>));
                    if (lua_isnil(ss, 2))
                    {
                        return type_pairs_iter(ss, typename Meta<T>::Members(), data, nullptr);
                    }
                    const auto hash = hash_fnv1a(lua_tostring(ss, 2));
                    return type_pairs_iter(ss, typename Meta<T>::Members(), data, &hash);
                }
            );
            lua_pushlightuserdata(state, luaL_testudata(state, 1, xalt::str_name_type_v<T>));
            luaL_getmetatable(state, xalt::str_name_type_v<T>);
            lua_setmetatable(state, -2);
            lua_pushnil(state);
            return 3;
        }

    private:
        template <typename... CType, typename... TRest>
        static void type_constructor(
            lua_State* state,
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

            if (match(state, std::make_index_sequence<sizeof...(CType)>()))
            {
                if constexpr (sizeof...(TRest) == 0)
                {
                    luaL_error(state, "[%s] can't be constructed with the provided arguments", xalt::str_name_type_v<T>);
                }
                else
                {
                    type_constructor(state, List<TRest...>());
                }
            }
        }

        template <auto member>
        static int type_method_call(lua_State* state)
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

            return fn(state, Args(), std::make_index_sequence<Args::size>());
        }

        template <xalt::literal l, auto p>
        static void type_get_key(lua_State* state, Method<l, p> /* member */)
        {
            lua_pushstring(state, nil::xalt::literal_v<l>);
        }

        template <xalt::literal l, auto p>
        static void type_get_key(lua_State* state, Property<l, p> /* member */)
        {
            lua_pushstring(state, nil::xalt::literal_v<l>);
        }

        template <xalt::literal l, auto p>
        static void type_get_member(lua_State* state, Method<l, p> /* member */, T* /* data */)
        {
            lua_pushcfunction(state, &type_method_call<p>);
        }

        template <xalt::literal l, auto p>
        static void type_get_member(lua_State* state, Property<l, p> /* member */, T* data)
        {
            TypeDef<decltype(data->*p)>::push(state, data->*p);
        }

        template <typename M, typename... TRest>
        static void type_get_members(
            lua_State* state,
            List<M, TRest...> /* props */,
            T* data,
            const char* key,
            std::uint32_t key_hash
        )
        {
            static constinit auto hash = hash_fnv1a(M());
            if (key_hash == hash)
            {
                type_get_member(state, M(), data);
            }
            else
            {
                type_get_members(state, List<TRest...>(), data, key, key_hash);
            }
        }

        template <xalt::literal l, auto p>
        static void type_set_member(lua_State* state, Method<l, p> /* member */, T* /* data */)
        {
            luaL_error(state, "[%s] member functions are not be replaceable", xalt::str_name_type_v<T>);
        }

        template <xalt::literal l, auto p>
        static void type_set_member(lua_State* state, Property<l, p> /* member */, T* data)
        {
            data->*p = TypeDef<decltype(data->*p)>::value(state, 3);
        }

        template <typename M, typename... TRest>
        static void type_set_members(
            lua_State* state,
            List<M, TRest...> /* props */,
            T* data,
            const char* key,
            std::uint32_t key_hash
        )
        {
            static constinit auto hash = hash_fnv1a(M());
            if (key_hash == hash)
            {
                type_set_member(state, M(), data);
            }
            else
            {
                type_set_members(state, List<TRest...>(), data, key, key_hash);
            }
        }

        template <typename M, typename... Rest>
        static int type_pairs_iter(
            lua_State* state,
            List<M, Rest...> /* members */,
            T* data,
            const std::uint32_t* key_hash
        )
        {
            if (key_hash == nullptr)
            {
                type_get_key(state, M());
                type_get_member(state, M(), data);
                return 2;
            }
            static constinit auto hash = hash_fnv1a(M());
            if (*key_hash == hash)
            {
                return type_pairs_iter(state, List<Rest...>(), data, nullptr);
            }
            if constexpr (sizeof...(Rest) == 0)
            {
                return 0;
            }
            else
            {
                return type_pairs_iter(state, List<Rest...>(), data, key_hash);
            }
        }

        static void type_get_members(
            lua_State* state,
            List<> /* props */,
            T* /* data */,
            const char* key,
            std::uint32_t /* key_hash */
        )
        {
            luaL_error(state, "[%s] member [%s] is unknown", xalt::str_name_type_v<T>, key);
        }

        static void type_set_members(
            lua_State* state,
            List<> /* props */,
            T* /* data */,
            const char* key,
            std::uint32_t /* key_hash */
        )
        {
            luaL_error(state, "[%s] member [%s] is unknown", xalt::str_name_type_v<T>, key);
        }

        static int type_pairs_iter(
            lua_State* /* state */,
            List<> /* members */,
            T* /* data */,
            const std::uint32_t* /* key_hash */
        )
        {
            return 0;
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
