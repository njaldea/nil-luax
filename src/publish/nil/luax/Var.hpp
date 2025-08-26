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

        friend TypeDef<Var>;
    };

    template <>
    struct TypeDef<Var>
    {
        static bool check(lua_State* state, int index) = delete;

        static Var value(lua_State* state, int index)
        {
            lua_pushvalue(state, index);
            return Var(std::make_shared<Ref>(state));
        }

        static void push(lua_State* /* state */, const Var& v)
        {
            v.ref->push();
        }

        static Var pull(const std::shared_ptr<Ref>& ref)
        {
            return Var(ref);
        }
    };

    template <typename T>
        requires(std::is_same_v<std::remove_cvref_t<T>, Var>)
    struct TypeDef<T&>
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
}
