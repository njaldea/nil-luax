#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nil/xlua.hpp>

TEST(xlua, global_vars)
{
    auto state = nil::xlua::State();

    state.set("v_boolean", true);
    state.set("v_number", 1.1);
    state.set("v_integer", 2);
    state.set("v_string", std::string("hello world"));

    ASSERT_EQ(true, state.get("v_boolean").as<bool>());
    ASSERT_EQ(1.1, state.get("v_number").as<double>());
    ASSERT_EQ(2, state.get("v_integer").as<int>());
    ASSERT_EQ("hello world", state.get("v_string").as<std::string>());

    state.set("v_boolean", false);
    state.set("v_number", 2.2);
    state.set("v_integer", 4);
    state.set("v_string", std::string("world hello"));

    ASSERT_EQ(false, state.get("v_boolean").as<bool>());
    ASSERT_EQ(2.2, state.get("v_number").as<double>());
    ASSERT_EQ(4, state.get("v_integer").as<int>());
    ASSERT_EQ("world hello", state.get("v_string").as<std::string>());
}

TEST(xlua, global_fn_args)
{
    testing::StrictMock<testing::MockFunction<void(bool, double, int, std::string)>> mock_call;
    testing::InSequence seq;

    {
        auto state = nil::xlua::State();
        state.set(
            "call",
            std::function<void(bool, double, int, const std::string&)>(
                [&mock_call](bool b, double d, int i, const std::string& s)
                { mock_call.Call(b, d, i, s); }
            )
        );

        auto fn = state.get("call").as<void(bool, double, int, const std::string&)>();
        static_assert(std::is_same_v<
                      decltype(fn),
                      std::function<void(bool, double, int, const std::string&)>>);

        EXPECT_CALL(mock_call, Call(true, 1.1, 2, "hello world")) //
            .Times(1)
            .RetiresOnSaturation();
        fn(true, 1.1, 2, "hello world");

        EXPECT_CALL(mock_call, Call(false, 2.2, 4, "world hello")) //
            .Times(1)
            .RetiresOnSaturation();
        fn(false, 2.2, 4, "world hello");
    }
    {
        auto state = nil::xlua::State();
        state.set(
            "call",
            [&mock_call](bool b, double d, int i, const std::string& s)
            { mock_call.Call(b, d, i, s); }
        );

        auto fn = state.get("call").as<void(bool, double, int, const std::string&)>();
        static_assert(std::is_same_v<
                      decltype(fn),
                      std::function<void(bool, double, int, const std::string&)>>);

        EXPECT_CALL(mock_call, Call(true, 1.1, 2, "hello world")) //
            .Times(1)
            .RetiresOnSaturation();
        fn(true, 1.1, 2, "hello world");

        EXPECT_CALL(mock_call, Call(false, 2.2, 4, "world hello")) //
            .Times(1)
            .RetiresOnSaturation();
        fn(false, 2.2, 4, "world hello");
    }
}

TEST(xlua, global_fn_return)
{
    testing::StrictMock<testing::MockFunction<bool()>> mock_bool;
    testing::StrictMock<testing::MockFunction<double()>> mock_number;
    testing::StrictMock<testing::MockFunction<int()>> mock_integer;
    testing::StrictMock<testing::MockFunction<std::string()>> mock_string;
    testing::InSequence seq;

    {
        auto state = nil::xlua::State();

        state.set("fn_bool", std::function<bool()>([&]() { return mock_bool.Call(); }));
        state.set("fn_number", std::function<double()>([&]() { return mock_number.Call(); }));
        state.set("fn_integer", std::function<int()>([&]() { return mock_integer.Call(); }));
        state.set("fn_string", std::function<std::string()>([&]() { return mock_string.Call(); }));

        auto fn_bool = state.get("fn_bool").as<bool()>();
        auto fn_number = state.get("fn_number").as<double()>();
        auto fn_integer = state.get("fn_integer").as<int()>();
        auto fn_string = state.get("fn_string").as<std::string()>();

        static_assert(std::is_same_v<decltype(fn_bool), std::function<bool()>>);
        static_assert(std::is_same_v<decltype(fn_number), std::function<double()>>);
        static_assert(std::is_same_v<decltype(fn_integer), std::function<int()>>);
        static_assert(std::is_same_v<decltype(fn_string), std::function<std::string()>>);

        EXPECT_CALL(mock_bool, Call()) //
            .Times(1)
            .WillOnce(testing::Return(true))
            .RetiresOnSaturation();
        EXPECT_EQ(true, fn_bool());
        EXPECT_CALL(mock_number, Call()) //
            .Times(1)
            .WillOnce(testing::Return(1.1))
            .RetiresOnSaturation();
        EXPECT_EQ(1.1, fn_number());
        EXPECT_CALL(mock_integer, Call()) //
            .Times(1)
            .WillOnce(testing::Return(2))
            .RetiresOnSaturation();
        EXPECT_EQ(2, fn_integer());
        EXPECT_CALL(mock_string, Call()) //
            .Times(1)
            .WillOnce(testing::Return("hello world"))
            .RetiresOnSaturation();
        EXPECT_EQ("hello world", fn_string());
    }

    {
        auto state = nil::xlua::State();

        state.set("fn_bool", [&]() { return mock_bool.Call(); });
        state.set("fn_number", [&]() { return mock_number.Call(); });
        state.set("fn_integer", [&]() { return mock_integer.Call(); });
        state.set("fn_string", [&]() { return mock_string.Call(); });

        auto fn_bool = state.get("fn_bool").as<bool()>();
        auto fn_number = state.get("fn_number").as<double()>();
        auto fn_integer = state.get("fn_integer").as<int()>();
        auto fn_string = state.get("fn_string").as<std::string()>();

        static_assert(std::is_same_v<decltype(fn_bool), std::function<bool()>>);
        static_assert(std::is_same_v<decltype(fn_number), std::function<double()>>);
        static_assert(std::is_same_v<decltype(fn_integer), std::function<int()>>);
        static_assert(std::is_same_v<decltype(fn_string), std::function<std::string()>>);

        EXPECT_CALL(mock_bool, Call()) //
            .Times(1)
            .WillOnce(testing::Return(true))
            .RetiresOnSaturation();
        EXPECT_EQ(true, fn_bool());
        EXPECT_CALL(mock_number, Call()) //
            .Times(1)
            .WillOnce(testing::Return(1.1))
            .RetiresOnSaturation();
        EXPECT_EQ(1.1, fn_number());
        EXPECT_CALL(mock_integer, Call()) //
            .Times(1)
            .WillOnce(testing::Return(2))
            .RetiresOnSaturation();
        EXPECT_EQ(2, fn_integer());
        EXPECT_CALL(mock_string, Call()) //
            .Times(1)
            .WillOnce(testing::Return("hello world"))
            .RetiresOnSaturation();
        EXPECT_EQ("hello world", fn_string());
    }
}

struct CustomType
{
    bool b;
    double d;
    int i;
    std::string s;
};

template <>
struct nil::xlua::Type<CustomType>
{
    using Constructors = nil::xlua::List<nil::xlua::Constructor<bool, double, int, std::string>>;

    using Members = nil::xlua::List<
        nil::xlua::Property<"b", &CustomType::b>,
        nil::xlua::Property<"d", &CustomType::d>,
        nil::xlua::Property<"i", &CustomType::i>,
        nil::xlua::Property<"s", &CustomType::s>>;
};

TEST(xlua, global_custom_type)
{
    auto state = nil::xlua::State();

    state.add_type<CustomType>("CustomType");
    state.run(R"(custom_value = CustomType(true, 1.1, 2, "hello world"))");

    CustomType object1 = state.get("custom_value");

    ASSERT_EQ(true, object1.b);
    ASSERT_EQ(1.1, object1.d);
    ASSERT_EQ(2, object1.i);
    ASSERT_EQ("hello world", object1.s);

    CustomType& object2 = state.get("custom_value");

    ASSERT_EQ(true, object2.b);
    ASSERT_EQ(1.1, object2.d);
    ASSERT_EQ(2, object2.i);
    ASSERT_EQ("hello world", object2.s);

    CustomType& object3 = state.get("custom_value");

    ASSERT_EQ(true, object3.b);
    ASSERT_EQ(1.1, object3.d);
    ASSERT_EQ(2, object3.i);
    ASSERT_EQ("hello world", object3.s);

    ASSERT_NE(&object1, &object2);
    ASSERT_NE(&object1, &object3);
    ASSERT_EQ(&object2, &object3);
}

TEST(xlua, global_custom_type_as_arg)
{
    auto state = nil::xlua::State();

    state.add_type<CustomType>("CustomType");
    state.run(R"(
        function get_b(custom_value)
            return custom_value.b
        end
        function get_d(custom_value)
            return custom_value.d
        end
        function get_i(custom_value)
            return custom_value.i
        end
        function get_s(custom_value)
            return custom_value.s
        end
    )");

    CustomType object1 = {true, 1.1, 2, "hello world"};
    {
        auto get_b = state.get("get_b").as<bool(CustomType)>();
        auto get_d = state.get("get_d").as<double(CustomType)>();
        auto get_i = state.get("get_i").as<int(CustomType)>();
        auto get_s = state.get("get_s").as<std::string(CustomType)>();

        ASSERT_EQ(true, get_b(object1));
        ASSERT_EQ(1.1, get_d(object1));
        ASSERT_EQ(2, get_i(object1));
        ASSERT_EQ("hello world", get_s(object1));
    }
    {
        auto get_b = state.get("get_b").as<bool(CustomType&)>();
        auto get_d = state.get("get_d").as<double(CustomType&)>();
        auto get_i = state.get("get_i").as<int(CustomType&)>();
        auto get_s = state.get("get_s").as<std::string(CustomType&)>();

        ASSERT_EQ(true, get_b(object1));
        ASSERT_EQ(1.1, get_d(object1));
        ASSERT_EQ(2, get_i(object1));
        ASSERT_EQ("hello world", get_s(object1));
    }
}
