#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nil/luax.hpp>

struct CustomTypeWithMethod
{
    testing::MockFunction<void(CustomTypeWithMethod*)>* mock;

    void call()
    {
        mock->Call(this);
    }
};

template <>
struct nil::luax::Meta<CustomTypeWithMethod>
{
    using Members = nil::luax::List<nil::luax::Method<"call", &CustomTypeWithMethod::call>>;
};

TEST(luax, custom_type_with_methods)
{
    auto state = nil::luax::State();

    state.add_type<CustomTypeWithMethod>();
    state.run(R"(
        function call(custom_value)
            return custom_value:call()
        end
    )");

    testing::StrictMock<testing::MockFunction<void(CustomTypeWithMethod*)>> mock;
    CustomTypeWithMethod object = {&mock};

    {
        auto call = state.get("call").as<void(CustomTypeWithMethod&)>();

        EXPECT_CALL(mock, Call(testing::Eq(&object))) //
            .Times(1)
            .RetiresOnSaturation();

        call(object);
    }
    {
        auto call = state.get("call").as<void(CustomTypeWithMethod)>();

        EXPECT_CALL(mock, Call(testing::Ne(&object))) //
            .Times(1)
            .RetiresOnSaturation();

        call(object);
    }
}
