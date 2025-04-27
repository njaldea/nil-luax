#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nil/luax.hpp>

#include <nil/xalt/noisy_type.hpp>

struct CustomType
{
};

template <>
struct nil::luax::Meta<CustomType>
{
};

TEST(luax, custom_type)
{
    auto state = nil::luax::State();

    state.add_type<CustomType>();

    CustomType value;
    state.set("custom_value", value);
    ASSERT_EQ(&value, &state.get("custom_value").as<CustomType&>());
}
