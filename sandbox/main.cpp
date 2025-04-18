#include <nil/clix.hpp>
#include <nil/clix/options.hpp>

#include <nil/xlua.hpp>

#include <iostream>

int run_from_file(std::string_view path)
{
    auto state = nil::xlua::State();

    state.load(path);

    const std::function<double(double, double)> add = state["add"];
    std::cout << add(10.0, 2.0) << std::endl;
    std::cout << add(12.0, 6.0) << std::endl;
    return 0;
}

int run_string()
{
    auto state = nil::xlua::State();

    state.run(R"(
        c = 100
        c_fun = nil
        function call()
            if c_fun ~= nil then 
                print("["..c_fun(1.0, 2.0).."]")
                c_fun = nil
                return false
            end
            return true
        end
        function add(a, b)
            return c + a + b
        end
        function sub(a, b)
            print("old sub")
            sub = function(a, b)
                print("new sub")
                return c + a + b
            end
            return c - (a - b)
        end
    )");

    state.set("c_fun", [](double value1, double value2) { return value1 * value2 * 2; });

    std::cout << state["call"].as<bool()>()() << std::endl;
    std::cout << state["call"].as<bool()>()() << std::endl;

    std::cout << "-----------\n";
    const auto add = state["add"].as<std::function<double(double, double)>>();
    const auto sub = state["sub"].as<double(double, double)>();
    std::cout << state["c"].as<double>() << ":" << add(10.0, 2.0) << std::endl;
    std::cout << state["c"].as<double>() << ":" << sub(12.0, 6.0) << std::endl;
    std::cout << state["c"].as<double>() << ":" << add(12.0, 6.0) << std::endl;
    std::cout << state["c"].as<double>() << ":" << sub(10.0, 2.0) << std::endl;
    std::cout << "-----------\n";

    state.gc();
    return 0;
}

int main(int argc, const char** argv)
{
    auto node = nil::clix::create_node();
    flag(node, "help", {.skey = 'h', .msg = "this help"});
    param(node, "file", {.skey = 'f', .msg = "lua script file path"});

    use(node,
        [](const nil::clix::Options& options)
        {
            if (flag(options, "help"))
            {
                help(options, std::cout);
                return 0;
            }

            if (has_value(options, "file"))
            {
                return run_from_file(param(options, "file"));
            }
            return run_string();
        });

    return run(node, argc, argv);
}
