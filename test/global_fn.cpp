#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nil/luax.hpp>

TEST(luax, global_fn_args)
{
    testing::StrictMock<testing::MockFunction<void(bool, double, int, std::string)>> mock_call;
    testing::InSequence seq;

    {
        auto state = nil::luax::State();
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
        auto state = nil::luax::State();
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

TEST(luax, global_fn_return)
{
    testing::StrictMock<testing::MockFunction<bool()>> mock_bool;
    testing::StrictMock<testing::MockFunction<double()>> mock_number;
    testing::StrictMock<testing::MockFunction<int()>> mock_integer;
    testing::StrictMock<testing::MockFunction<std::string()>> mock_string;
    testing::InSequence seq;

    {
        auto state = nil::luax::State();

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
        auto state = nil::luax::State();

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
