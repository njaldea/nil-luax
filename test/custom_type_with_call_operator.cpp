#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nil/luax.hpp>

struct CustomTypeWithCallOperator
{
    testing::MockFunction<void(CustomTypeWithCallOperator*)>* mock;

    void operator()()
    {
        mock->Call(this);
    }
};

template <>
struct nil::luax::Meta<CustomTypeWithCallOperator>
{
};

TEST(luax, custom_type_with_call_operator)
{
    auto state = nil::luax::State();

    state.add_type<CustomTypeWithCallOperator>();
    state.run(R"(
        function call(custom_value)
            return custom_value()
        end
    )");

    testing::StrictMock<testing::MockFunction<void(CustomTypeWithCallOperator*)>> mock;
    CustomTypeWithCallOperator object = {&mock};

    {
        auto call = state.get("call").as<void(CustomTypeWithCallOperator&)>();

        EXPECT_CALL(mock, Call(testing::Eq(&object))) //
            .Times(1)
            .RetiresOnSaturation();

        call(object);
    }
    {
        auto call = state.get("call").as<void(CustomTypeWithCallOperator)>();

        EXPECT_CALL(mock, Call(testing::Ne(&object))) //
            .Times(1)
            .RetiresOnSaturation();

        call(object);
    }
}
