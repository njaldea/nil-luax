#include "Ref.hpp"
#include "TypeDef.hpp"

#include <lua.h>
#include <memory>

namespace nil::luax
{
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
            requires(is_value_type<T> || is_std_fn<T>)
        // NOLINTNEXTLINE
        operator T() const
        {
            return as<T>();
        }

        template <typename T>
            requires(!is_value_type<std::remove_cvref_t<T>> && !is_std_fn<std::remove_cvref_t<T>>)
        // NOLINTNEXTLINE
        operator T&() const
        {
            return as<T>();
        }

        operator std::string_view() const = delete;
        operator const char*() const = delete;

        template <typename T>
        decltype(auto) as() const
        {
            return TypeDef<T>::pull(ref);
        }

    private:
        std::shared_ptr<Ref> ref;
    };

    template <>
    struct TypeDef<Var>
    {
        static bool check(lua_State* state, int index)
        {
            return luaL_checkudata(state, index, xalt::str_name_type_v<Var>) != nullptr;
        }

        static Var value(lua_State* state, int index)
        {
            lua_pushvalue(state, index);
            return Var(std::make_shared<Ref>(state));
        }

        static void push(lua_State* state, Var callable)
        {
            auto* data = static_cast<Var*>(lua_newuserdata(state, sizeof(Var)));
            new (data) Var(std::move(callable));
        }

        static auto& pull(const std::shared_ptr<Ref>& ref)
        {
            auto* state = ref->push();
            auto* ptr = static_cast<Var*>(lua_touserdata(state, -1));
            lua_pop(state, 1);
            return *ptr;
        }
    };
}
