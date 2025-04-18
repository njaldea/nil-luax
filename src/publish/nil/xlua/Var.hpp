#include "Ref.hpp"
#include "TypeDef.hpp"

#include <memory>

namespace nil::xlua
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
        // NOLINTNEXTLINE
        operator T() const
        {
            return as<T>();
        }

        template <typename T>
        auto as() const
        {
            return TypeDef<T>::pull(ref);
        }

    private:
        std::shared_ptr<Ref> ref;
    };
}
