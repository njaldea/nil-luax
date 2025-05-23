#include <nil/clix.hpp>
#include <nil/clix/options.hpp>

#include <nil/luax.hpp>
#include <nil/xalt/literal.hpp>
#include <nil/xalt/noisy_type.hpp>
#include <nil/xalt/str_name.hpp>
#include <nil/xalt/tlist.hpp>

#include <iostream>

int run_from_file(std::string_view path)
{
    auto state = nil::luax::State();

    state.load(path);

    const std::function<double(double, double)> add = state.get("add");
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

    void do_it() const
    {
        std::cout << "do_it:" << name << ":" << age << ":" << job << ":" << city << std::endl;
    }
};

template <>
struct nil::luax::Meta<Person>
{
    using Constructors = nil::luax::List<                                  //
        nil::luax::Constructor<>,                                          //
        nil::luax::Constructor<std::string, int, std::string, std::string> //
        >;

    using Members = nil::luax::List<                //
        nil::luax::Property<"name", &Person::name>, //
        nil::luax::Property<"age", &Person::age>,   //
        nil::luax::Property<"job", &Person::job>,   //
        nil::luax::Property<"city", &Person::city>, //
        nil::luax::Method<"do_it", &Person::do_it>  //
        >;
};

int run_string()
{
    auto state = nil::luax::State();
    state.add_type<Person>("Person");

    state.run(R"(
        person = Person()

        function call(callable)
            return callable(2.0, 2.0)
        end

        function call_2()
            print(person)

            -- Access table values
            print("Name:", person.name)
            print("Age:", person.age)
            print("Job:", person["job"])

            -- Add a new key-value pair
            person.city = "New York"
            print("City:", person.city)

            -- Iterate through the table
            print("-----")
            for key, value in pairs(person) do
                print(key, ": " .. tostring(value))
            end
            print("-----")
            print(person)
            print("-----")

            -- person:do_it()
            return person;
        end
    )");

    auto call = state.get("call_2").as<nil::luax::Var()>();

    struct HelloWorld
    {
        HelloWorld()
        {
            std::cout << this << std::endl;
        }

        void operator()() const
        {
            std::cout << this << std::endl;
            std::cout << this << " : hello world\n";
        }

        nil::xalt::noisy_type<"test"> t;
    };

    state.set("hello", HelloWorld());

    state.get("hello").as<void()>()();
    const HelloWorld& world = state.get("hello");
    world();

    auto& person = state.get("person").as<Person&>();
    std::cout << &person << std::endl;
    auto v = call();
    std::cout << &v.as<Person&>() << std::endl;

    std::cout << "-------\n";
    std::cout << person.name << std::endl;
    std::cout << person.age << std::endl;
    std::cout << person.job << std::endl;
    std::cout << person.city << std::endl;

    std::cout << "-------\n";
    person.name = "njla";
    call();

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
