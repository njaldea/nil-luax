#include <nil/clix.hpp>
#include <nil/clix/options.hpp>
#include <nil/gate.hpp>

#include <nil/luax.hpp>
#include <nil/xalt/literal.hpp>
#include <nil/xalt/noisy_type.hpp>
#include <nil/xalt/str_name.hpp>
#include <nil/xalt/tlist.hpp>

#include <iostream>

int run_from_file(std::string_view path)
{
    auto state = nil::luax::State();
    state.open_libs();
    state.load(path);

    const std::function<double(double, double)> add = state.get("add");
    std::cout << add(10.0, 2.0) << std::endl;
    std::cout << add(12.0, 6.0) << std::endl;
    return 0;
}

struct Callable
{
    int operator()() const
    {
        return 0;
    }
};

int run()
{
    nil::luax::State lua_state;
    lua_state.open_libs();
    lua_state.set(
        "foo",
        [](const nil::luax::Var& var, const int& v)
        {
            const auto [a, b] = var.as<std::function<std::tuple<int, int>(int)>>()(v);
            std::cout << a << ":" << b << std::endl;
        }
    );
    lua_state.run(R"(
        function bar(a)
            return a + 1, a + 2
        end
        foo(bar, 1)
    )");
    return 0;
}

int main(int argc, const char** argv)
{
    auto node = nil::clix::create_node();
    flag(node, "help", {.skey = 'h', .msg = "this help"});

    use(node,
        [](const nil::clix::Options& options)
        {
            if (flag(options, "help"))
            {
                help(options, std::cout);
                return 0;
            }

            return run();
        });

    return run(node, argc, argv);
}
