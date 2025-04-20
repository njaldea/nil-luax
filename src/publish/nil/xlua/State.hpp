#pragma once

#include "TypeDef.hpp"
#include "Var.hpp"
#include "error.hpp"

#include <nil/xalt/fn_sign.hpp>
#include <nil/xalt/literal.hpp>
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
#include <utility>

namespace nil::xlua
{
    template <typename... Args>
    struct Constructor
    {
    };

    template <nil::xalt::literal name, auto ptr_to_member>
    struct Property
    {
    };

    template <typename... Types>
    struct List
    {
    };

    template <typename T>
    struct Type;

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
            requires(std::is_pointer_v<MemFun> && std::is_same_v<typename nil::xalt::fn_sign<MemFun>::class_type, C>)
        void set(std::string_view name, MemFun fun, C* c)
        {
            set(name,
                [c, fun]<typename... Args>(Args... args)
                { return (c->*fun)(args...); }(typename nil::xalt::fn_sign<MemFun>::arg_types()));
        }

        template <typename FreeFun>
            requires(std::is_pointer_v<FreeFun> && std::is_same_v<typename nil::xalt::fn_sign<FreeFun>::class_type, void>)
        void set(std::string_view name, FreeFun fun)
        {
            auto lambda = [fun]<typename... Args>(nil::xalt::tlist_types<Args...> /* arg types */) {
                return [fun](Args... args) { return fun(args...); };
            }(typename nil::xalt::fn_sign<FreeFun>::arg_types());
            set(name, lambda);
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
            lua_register(
                state,
                name.data(),
                [](lua_State* s)
                {
                    type_construct<T>(s, typename Type<T>::Constructors());
                    return 1;
                } //
            );

            luaL_newmetatable(state, nil::xalt::str_name_type_v<T>);
            lua_pushcfunction(state, &TypeDefCommon<T>::del);
            lua_setfield(state, -2, "__gc");
            lua_pushcfunction(
                state,
                [](lua_State* s)
                {
                    T* data = static_cast<T*>(luaL_checkudata(s, 1, nil::xalt::str_name_type_v<T>));
                    type_get_prop(data, luaL_checkstring(s, 2), s, typename Type<T>::Props());
                    return 1;
                }
            );
            lua_setfield(state, -2, "__index");
            lua_pushcfunction(
                state,
                [](lua_State* s)
                {
                    T* data = static_cast<T*>(luaL_checkudata(s, 1, nil::xalt::str_name_type_v<T>));
                    type_set_prop(data, luaL_checkstring(s, 2), s, typename Type<T>::Props());
                    return 1;
                }
            );
            lua_setfield(state, -2, "__newindex");
            // lua_pushcfunction(
            //     state,
            //     [](lua_State* s)
            //     {
            //         T* data = static_cast<T*>(luaL_checkudata(s, 1,
            //         nil::xalt::str_name_type_v<T>)); type_pair_prop(data, luaL_checkstring(s, 2),
            //         s, typename Type<T>::Props()); return 1;
            //     }
            // );
            // lua_setfield(state, -2, "__pairs");
        }

    private:
        lua_State* state;

        template <typename T, nil::xalt::literal l, auto r, typename... TRest>
        static void type_get_prop(
            T* data,
            const char* key,
            lua_State* s,
            List<Property<l, r>, TRest...> /* props */
        )
        {
            if (std::string_view(nil::xalt::literal_v<l>) == key)
            {
                TypeDef<decltype(data->*r)>::push(s, data->*r);
                return;
            }
            if constexpr (sizeof...(TRest) == 0)
            {
                lua_pushnil(s);
            }
            else
            {
                type_get_prop(data, key, s, List<TRest...>());
            }
        }

        template <typename T, nil::xalt::literal l, auto r, typename... TRest>
        static void type_set_prop(
            T* data,
            const char* key,
            lua_State* s,
            List<Property<l, r>, TRest...> /* props */
        )
        {
            if (std::string_view(nil::xalt::literal_v<l>) == key)
            {
                data->*r = TypeDef<decltype(data->*r)>::value(s, 3);
                return;
            }
            if constexpr (sizeof...(TRest) == 0)
            {
                throw_error(s);
            }
            else
            {
                type_set_prop(data, key, s, List<TRest...>());
            }
        }

        template <typename T, typename... CType, typename... TRest>
        static void type_construct(
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
                        luaL_getmetatable(s, nil::xalt::str_name_type_v<T>);
                        lua_setmetatable(s, -2);
                        return true;
                    }
                    return false;
                }(std::make_index_sequence<sizeof...(CType)>()))
            {
                if constexpr (sizeof...(TRest) == 0)
                {
                    throw_error(s);
                }
                else
                {
                    type_construct<T>(s, List<TRest...>());
                }
            }
        }
    };
}
