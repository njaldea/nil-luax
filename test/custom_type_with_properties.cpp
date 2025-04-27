#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nil/luax.hpp>

struct CustomTypeWithProperties
{
    bool b;
    double d;
    int i;
    std::string s;
};

template <>
struct nil::luax::Meta<CustomTypeWithProperties>
{
    using Constructors = nil::luax::List<nil::luax::Constructor<bool, double, int, std::string>>;

    using Members = nil::luax::List<
        nil::luax::Property<"b", &CustomTypeWithProperties::b>,
        nil::luax::Property<"d", &CustomTypeWithProperties::d>,
        nil::luax::Property<"i", &CustomTypeWithProperties::i>,
        nil::luax::Property<"s", &CustomTypeWithProperties::s>>;
};

TEST(luax, custom_type_with_properties_using_constructor)
{
    auto state = nil::luax::State();

    state.add_type<CustomTypeWithProperties>("CustomTypeWithProperties");
    state.run(R"(custom_value = CustomTypeWithProperties(true, 1.1, 2, "hello world"))");

    CustomTypeWithProperties object1 = state.get("custom_value");

    ASSERT_EQ(true, object1.b);
    ASSERT_EQ(1.1, object1.d);
    ASSERT_EQ(2, object1.i);
    ASSERT_EQ("hello world", object1.s);

    CustomTypeWithProperties& object2 = state.get("custom_value");

    ASSERT_EQ(true, object2.b);
    ASSERT_EQ(1.1, object2.d);
    ASSERT_EQ(2, object2.i);
    ASSERT_EQ("hello world", object2.s);

    CustomTypeWithProperties& object3 = state.get("custom_value");

    ASSERT_EQ(true, object3.b);
    ASSERT_EQ(1.1, object3.d);
    ASSERT_EQ(2, object3.i);
    ASSERT_EQ("hello world", object3.s);

    ASSERT_NE(&object1, &object2);
    ASSERT_NE(&object1, &object3);
    ASSERT_EQ(&object2, &object3);
}

TEST(luax, custom_type_with_properties_as_fn_arg)
{
    testing::StrictMock<testing::MockFunction<void(CustomTypeWithProperties*)>> mock;
    testing::InSequence s;

    auto state = nil::luax::State();

    state.add_type<CustomTypeWithProperties>("CustomTypeWithProperties");
    state.set("inspect", [&](CustomTypeWithProperties& object) { mock.Call(&object); });
    state.run(R"(
        function get_b(custom_value)
            inspect(custom_value)
            return custom_value.b
        end
        function get_d(custom_value)
            inspect(custom_value)
            return custom_value.d
        end
        function get_i(custom_value)
            inspect(custom_value)
            return custom_value.i
        end
        function get_s(custom_value)
            inspect(custom_value)
            return custom_value.s
        end
    )");

    CustomTypeWithProperties object1 = {true, 1.1, 2, "hello world"};
    {
        EXPECT_CALL(mock, Call(testing::Ne(&object1))) //
            .Times(4)
            .RetiresOnSaturation();
        auto get_b = state.get("get_b").as<bool(CustomTypeWithProperties)>();
        auto get_d = state.get("get_d").as<double(CustomTypeWithProperties)>();
        auto get_i = state.get("get_i").as<int(CustomTypeWithProperties)>();
        auto get_s = state.get("get_s").as<std::string(CustomTypeWithProperties)>();

        ASSERT_EQ(true, get_b(object1));
        ASSERT_EQ(1.1, get_d(object1));
        ASSERT_EQ(2, get_i(object1));
        ASSERT_EQ("hello world", get_s(object1));
    }
    {
        EXPECT_CALL(mock, Call(testing::Eq(&object1))) //
            .Times(4)
            .RetiresOnSaturation();
        auto get_b = state.get("get_b").as<bool(CustomTypeWithProperties&)>();
        auto get_d = state.get("get_d").as<double(CustomTypeWithProperties&)>();
        auto get_i = state.get("get_i").as<int(CustomTypeWithProperties&)>();
        auto get_s = state.get("get_s").as<std::string(CustomTypeWithProperties&)>();

        ASSERT_EQ(true, get_b(object1));
        ASSERT_EQ(1.1, get_d(object1));
        ASSERT_EQ(2, get_i(object1));
        ASSERT_EQ("hello world", get_s(object1));
    }
}
