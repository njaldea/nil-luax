# nil-lua (nil::luax)

Modern C++20 header-only bindings for Lua with a tiny, type-safe API.

- Header-only CMake INTERFACE target: `nil::luax`
- Simple globals get/set, call Lua functions as `std::function`
- Expose C++ lambdas/functions to Lua
- Define and expose C++ user types (constructors, properties, methods)
- RAII for Lua references and exceptions with readable messages

## Install and use

### CMake (recommended)

- Build and install:
  1) Configure and build as usual (vcpkg optional). This project installs a package config `nil-lua-config.cmake`.
  2) Run the install step to place headers and CMake config files.

- Consume from another project:

```cmake
find_package(nil-lua CONFIG REQUIRED)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE nil::luax)
```

Include in code:

```cpp
#include <nil/luax.hpp>
using namespace nil::luax;
```

Dependencies: Lua (C library) and `nil-xalt` (small header utilities). These are linked transitively by `nil::luax`. A `vcpkg.json` is provided if you prefer vcpkg.

## Quick start

### Run Lua and call a function

```cpp
#include <nil/luax.hpp>
#include <functional>
using namespace nil::luax;

int main() {
    State L;
    L.open_libs();
    L.run(R"(
        function add(a, b) return a + b end
    )");

    std::function<double(double,double)> add = L.get("add");
    return (add(10.0, 2.0) == 12.0) ? 0 : 1;
}
```

### Expose a C++ function/lambda to Lua

```cpp
State L;
L.open_libs();
L.set("say", [](const std::string& s) { /* print/log */ });
L.run("say('hello from lua')");
```

### Define and expose a C++ user type

```cpp
struct Vec2 {
    double x{0}
    double y{0};
    void translate(double dx, double dy)
    {
        x += dx;
        y += dy;
    }
};

template<> struct nil::luax::Meta<Vec2> {
    using Constructors = nil::luax::List<nil::luax::Constructor<double,double>>;
    using Members = nil::luax::List<
        nil::luax::Property<"x", &Vec2::x>,
        nil::luax::Property<"y", &Vec2::y>,
        nil::luax::Method<"translate", &Vec2::translate>
    >;
};

State L;
L.open_libs();
L.add_type<Vec2>("Vec2");
L.run(R"(
    v = Vec2(1.0, 2.0)
    v:translate(3.0, 4.0) -- x=4, y=6
)");

// Read back from Lua
auto& v = L.get("v").as<Vec2&>();
```

### Get/Set globals and dynamic Var

```cpp
State L;
L.set("v_boolean", true);
bool b = L.get("v_boolean").as<bool>();

// Functions can be pulled as signatures too:
auto fn = L.get("add").as<double(double,double)>(); // std::function
```

## API overview

- `class State`
  - `open_libs()` – open standard Lua libraries
  - `load(path)` / `run(script)` – run file or string
  - `get(name) -> Var` – retrieve a global
  - `set(name, value/callable)` – set a global (values, lambdas, `std::function`, free/member functions)
  - `add_type<T>()` – register metatable for user type `T`
  - `add_type<T>(name)` – also registers a constructor function `name(...)`
  - `gc()` – force GC

- `struct Var` – reference-like handle to a Lua value
  - `.as<T>()` – convert to C++ type or callable
  - Implicit conversions enabled for value/callable types; string-view/char* implicit conversions are disabled to avoid lifetime bugs

- User types via `Meta<T>` specialization
  - `using Constructors = List<Constructor<...>, ...>`
  - `using Members = List<Property<"name", &T::field>, Method<"name", &T::method>, ...>`
  - Supported metamethods: `__index`, `__newindex`, `__pairs`, `__call` (when `T::operator()` exists), `__close` (RAII)

## Type mapping

- Values: `bool`, integral, floating-point, `std::string`, `std::string_view`, `const char*`
- Callables: `std::function<R(Args...)>`, lambdas and functors (captured via upvalue)
- User types: by reference or value with automatic metatable setup after `add_type<T>()`

Errors in Lua/C API calls throw `std::invalid_argument` with the Lua error message.

TODO:
- lua table to `std::unordered_map<std::string, Var>`.

## License

SPDX-License-Identifier: CC-BY-NC-ND-4.0 — see `LICENSE`.
