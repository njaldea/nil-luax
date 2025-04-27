project(luax)

find_package(nil-xalt CONFIG REQUIRED)
find_package(Lua REQUIRED)

add_library(
    ${PROJECT_NAME} INTERFACE
    publish/nil/luax.hpp
    publish/nil/luax/error.hpp
    publish/nil/luax/Ref.hpp
    publish/nil/luax/State.hpp
    publish/nil/luax/TypeDef.hpp
    publish/nil/luax/UserType.hpp
    publish/nil/luax/Var.hpp
)

target_link_libraries(
    ${PROJECT_NAME} INTERFACE
    nil::xalt
    ${LUA_LIBRARIES}
)

target_include_directories(
    ${PROJECT_NAME}
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/publish>
    $<BUILD_INTERFACE:${LUA_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

nil_install_headers()
nil_install_targets(${PROJECT_NAME})
