#include <nil/clix.hpp>
#include <nil/clix/options.hpp>

#include <nil/xalt/checks.hpp>
#include <nil/xalt/literal.hpp>
#include <nil/xalt/tlist.hpp>
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

struct Person
{
    std::string name = "default_name";
    int age = 0;
    std::string job = "default_job";
    std::string city = "default_city";
};

template <>
struct nil::xlua::Type<Person>
{
    using Constructors = nil::xlua::List<                                  //
        nil::xlua::Constructor<>,                                          //
        nil::xlua::Constructor<std::string, int, std::string, std::string> //
        >;

    using Props = nil::xlua::List<              //
        nil::xlua::Prop<"name", &Person::name>, //
        nil::xlua::Prop<"age", &Person::age>,   //
        nil::xlua::Prop<"job", &Person::job>,   //
        nil::xlua::Prop<"city", &Person::city>  //
        >;
};

int run_string()
{
    auto state = nil::xlua::State();
    state.add_type<Person>("Person");

    state.run(R"(
        function call(callable)
            return callable(2.0, 2.0)
        end

        function call_2()
            -- Create a table
            local person = Person()

            -- Access table values
            print("Name:", person.name)
            print("Age:", person.age)
            print("Job:", person["job"])

            -- Add a new key-value pair
            person.city = "New York"
            print("City:", person.city)

            -- Iterate through the table
            for key, value in pairs(person) do
                print(key .. ": " .. tostring(value))
            end
        end
    )");

    // auto callable = state["call"].as<bool(std::function<bool(double, double)>)>();
    // std::cout << callable([](double a, double b) { return a == b; }) << std::endl;

    state["call_2"].as<void()>()();

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
