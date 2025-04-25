#include <gtest/gtest.h>

#include <nil/xlua.hpp>

TEST(xlua, global_vars)
{
    auto state = nil::xlua::State();

    state.set("v_number", 1.1);
    state.set("v_integer", 2);
    state.set("v_string", std::string("hello world"));
    state.set("v_boolean", true);

    ASSERT_EQ(1.1, state.get("v_number").as<double>());
    ASSERT_EQ(2, state.get("v_integer").as<int>());
    ASSERT_EQ("hello world", state.get("v_string").as<std::string>());
    ASSERT_EQ(true, state.get("v_boolean").as<bool>());

    state.set("v_number", 2.2);
    state.set("v_integer", 4);
    state.set("v_string", std::string("world hello"));
    state.set("v_boolean", false);

    ASSERT_EQ(2.2, state.get("v_number").as<double>());
    ASSERT_EQ(4, state.get("v_integer").as<int>());
    ASSERT_EQ("world hello", state.get("v_string").as<std::string>());
    ASSERT_EQ(false, state.get("v_boolean").as<bool>());
}
