#include <nil/clix.hpp>
#include <nil/clix/options.hpp>
#include <nil/xalt/checks.hpp>
#include <nil/xalt/fn_sign.hpp>
#include <nil/xalt/literal.hpp>
#include <nil/xalt/noisy_type.hpp>
#include <nil/xalt/str_name.hpp>

extern "C"
{
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <iostream>
#include <nil/xalt/tlist.hpp>
#include <stdexcept>
#include <type_traits>

namespace nil::xlua
{
    class Ref
    {
    public:
        explicit Ref(lua_State* init_state)
            : state(init_state)
            , ref(luaL_ref(state, LUA_REGISTRYINDEX))
        {
        }

        Ref(Ref&&) = delete;
        Ref(const Ref&) = delete;
        Ref& operator=(Ref&&) = delete;
        Ref& operator=(const Ref&) = delete;

        ~Ref()
        {
            luaL_unref(state, LUA_REGISTRYINDEX, ref);
        }

        lua_State* push()
        {
            lua_rawgeti(state, LUA_REGISTRYINDEX, ref);
            return state;
        }

    private:
        lua_State* state;
        int ref;
    };

    void throw_error(lua_State* state)
    {
        throw std::invalid_argument("Error: " + std::string(lua_tostring(state, -1)));
    }

    template <typename T>
    struct TypeMap;

    template <>
    struct TypeMap<bool> final
    {
        static void push(lua_State* state, bool value)
        {
            lua_pushboolean(state, value ? 1 : 0);
        }

        static void check(lua_State* state, int index)
        {
            if (lua_isboolean(state, index))
            {
                throw_error(state);
            }
        }

        static bool pull(lua_State* state, int index)
        {
            auto value = lua_toboolean(state, index) > 0;
            lua_pop(state, 1);
            return value;
        }

        static bool cast(const std::shared_ptr<Ref>& ref)
        {
            auto* state = ref->push();
            check(state, -1);
            return pull(state, -1);
        }
    };

    template <typename T>
        requires(std::is_floating_point_v<T>)
    struct TypeMap<T>
    {
        static void push(lua_State* state, T value)
        {
            lua_pushnumber(state, value);
        }

        static void check(lua_State* state, int index)
        {
            if (lua_isnumber(state, index) == 0)
            {
                throw_error(state);
            }
        }

        static T pull(lua_State* state, int index)
        {
            auto value = T(lua_tonumber(state, index));
            lua_pop(state, 1);
            return value;
        }

        static T cast(const std::shared_ptr<Ref>& ref)
        {
            auto* state = ref->push();
            check(state, -1);
            return pull(state, -1);
        }
    };

    template <typename T>
        requires(std::is_integral_v<T>)
    struct TypeMap<T>
    {
        static void push(lua_State* state, T value)
        {
            lua_pushinteger(state, value);
        }

        static void check(lua_State* state, int index)
        {
            if (lua_isinteger(state, index) == 0)
            {
                throw_error(state);
            }
        }

        static T pull(lua_State* state, int index)
        {
            auto value = T(lua_tointeger(state, index));
            lua_pop(state, 1);
            return value;
        }

        static T cast(const std::shared_ptr<Ref>& ref)
        {
            auto* state = ref->push();
            check(state, -1);
            return pull(state, -1);
        }
    };

    template <typename T>
        requires(std::is_same_v<std::string, T> || std::is_same_v<std::string_view, T>)
    struct TypeMap<T>
    {
        static void push(lua_State* state, const T& value)
        {
            lua_pushlstring(state, value.data(), value.size());
        }

        static void check(lua_State* state, int index)
        {
            if (lua_isstring(state, index) == 0)
            {
                throw_error(state);
            }
        }

        static std::string pull(lua_State* state, int index)
        {
            auto value = std::string(lua_tostring(state, index));
            lua_pop(state, 1);
            return value;
        }

        static T cast(const std::shared_ptr<Ref>& ref)
        {
            auto* state = ref->push();
            check(state, -1);
            return pull(state, -1);
        }
    };

    template <typename R, typename... Args>
    struct TypeMap<R(Args...)>
    {
        static void check(lua_State* state, int index)
        {
            if (!lua_isfunction(state, index))
            {
                throw_error(state);
            }
        }

        static std::function<R(Args...)> cast(std::shared_ptr<Ref> ref)
        {
            return [ref = std::move(ref)](Args... args)
            {
                auto* state = ref->push();
                check(state, -1);
                (TypeMap<Args>::push(state, args), ...);

                constexpr auto return_size = std::is_same_v<R, void> ? 0 : 1;
                if (lua_pcall(state, sizeof...(Args), return_size, 0) != LUA_OK)
                {
                    throw_error(state);
                }

                if constexpr (!std::is_same_v<void, R>)
                {
                    TypeMap<R>::check(state, -1);
                    return TypeMap<R>::pull(state, -1);
                }
            };
        }
    };

    template <typename T>
        requires requires() { &T::operator(); }
    struct TypeMap<T>: TypeMap<typename nil::xalt::fn_sign<T>::type>
    {
    private:
        using fn_sign = nil::xalt::fn_sign<T>;
        using return_type = fn_sign::return_type;
        using arg_types = fn_sign::arg_types;

    public:
        static void push(lua_State* state, T callable)
        {
            using type = T;
            auto* data = static_cast<type*>(lua_newuserdata(state, sizeof(type)));
            new (data) type(std::move(callable));

            lua_newtable(state);
            lua_pushcfunction(
                state,
                [](lua_State* s)
                {
                    static_cast<type*>(lua_touserdata(s, 1))->~type();
                    return 0;
                }
            );
            lua_setfield(state, -2, "__gc");
            lua_setmetatable(state, -2);

            lua_pushcclosure(
                state,
                [](lua_State* s) -> int
                {
                    auto* ptr = static_cast<type*>(lua_touserdata(s, lua_upvalueindex(1)));
                    (*ptr)();
                    return return_size();
                },
                1
            );
        }

    private:
        template <typename... A>
        consteval static auto arg_size(nil::xalt::tlist_types<A...> /* types */) -> int
        {
            return sizeof...(A);
        }

        consteval static auto return_size() -> int
        {
            return std::is_same_v<void, return_type> ? 0 : 1;
        }
    };

    struct Var
    {
    public:
        explicit Var(std::shared_ptr<Ref> init_ref)
            : ref(std::move(init_ref))
        {
        }

        Var(Var&&) = default;
        Var(const Var&) = default;
        Var& operator=(Var&&) = default;
        Var& operator=(const Var&) = default;

        ~Var() noexcept = default;

        template <typename T>
        // NOLINTNEXTLINE
        operator T() const
        {
            return as<T>();
        }

        template <typename T>
        auto as() const
        {
            return TypeMap<T>::cast(ref);
        }

    private:
        std::shared_ptr<Ref> ref;
    };

    class Lua final
    {
    public:
        Lua()
            : state(luaL_newstate())
        {
            luaL_openlibs(state);
        }

        Lua(Lua&&) = default;
        Lua& operator=(Lua&&) = default;

        Lua(const Lua&) = delete;
        Lua& operator=(const Lua&) = delete;

        ~Lua() noexcept
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
            TypeMap<T>::push(state, std::move(fn));
            lua_setglobal(state, name.data());
        }

        template <typename C, typename MemFun>
            requires(std::is_same_v<typename nil::xalt::fn_sign<MemFun>::class_type, C>)
        void set(std::string_view name, MemFun fun, C* c)
        {
            auto fn = wrap<typename nil::xalt::fn_sign<MemFun>::return_type>(
                fun,
                c,
                typename nil::xalt::fn_sign<MemFun>::arg_types()
            );
            TypeMap<decltype(fn)>::push(state, std::move(fn));
            lua_setglobal(state, name.data());
        }

        void gc()
        {
            lua_gc(state, LUA_GCCOLLECT, 0);
        }

    private:
        lua_State* state;

        template <typename R, typename C, typename MemFun, typename... A>
        auto wrap(MemFun fun, C* c, nil::xalt::tlist_types<A...> /* arg types */)
        {
            return [c, fun](A... args) { return (c->*fun)(args...); };
        }
    };
}

int run_from_file(std::string_view path)
{
    auto lua = nil::xlua::Lua();

    lua.load(path);

    const std::function<double(double, double)> add = lua["add"];
    std::cout << add(10.0, 2.0) << std::endl;
    std::cout << add(12.0, 6.0) << std::endl;
    return 0;
}

int run_string()
{
    auto lua = nil::xlua::Lua();

    lua.run(R"(
        c = 100
        c_fun = nil
        function call()
            if c_fun ~= nil then
                c_fun()
                c_fun = nil
            end
            if m_fun ~= nil then
                m_fun()
            end
        end
        function add(a, b)
            return c + a + b
        end
        function sub(a, b)
            print("old sub")
            sub = function(a, b)
                print("new sub")
                return c + a + b
            end
            return c - (a - b)
        end
    )");

    lua.set(
        "c_fun",
        [n = std::make_shared<nil::xalt::noisy_type<"asd">>()]() {
            std::cout << __FILE__ << ':' << __LINE__ << ':' << (const char*)(__FUNCTION__)
                      << std::endl;
        }
    );

    struct A
    {
        void bar()
        {
            std::cout << __FILE__ << ':' << __LINE__ << ':' << (const char*)(__FUNCTION__)
                      << std::endl;
        }
    };

    A a;
    lua.set("m_fun", &A::bar, &a);

    {
        auto call = lua["call"].as<void()>();
        call();
    }

    std::cout << "-----------\n";

    const auto add = lua["add"].as<std::function<double(double, double)>>();
    const auto sub = lua["sub"].as<double(double, double)>();
    std::cout << lua["c"].as<double>() << ":" << add(10.0, 2.0) << std::endl;
    std::cout << lua["c"].as<double>() << ":" << sub(12.0, 6.0) << std::endl;
    // lua.var<double>("c", 150);
    std::cout << lua["c"].as<double>() << ":" << add(12.0, 6.0) << std::endl;
    std::cout << lua["c"].as<double>() << ":" << sub(10.0, 2.0) << std::endl;

    std::cout << "-----------\n";

    lua.gc();
    return 0;
}

int main(int argc, const char** argv)
{
    auto node = nil::clix::create_node();
    flag(node, "help", {.skey = 'h', .msg = "this help"});
    param(node, "file", {.skey = 'f', .msg = "lua script file path"});

    use(node,
        [](const nil::clix::Options& options)
        {
            if (flag(options, "help"))
            {
                help(options, std::cout);
                return 0;
            }

            if (has_value(options, "file"))
            {
                return run_from_file(param(options, "file"));
            }
            return run_string();
        });

    return run(node, argc, argv);
}
