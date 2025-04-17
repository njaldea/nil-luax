project(sandbox)

find_package(nil-clix CONFIG REQUIRED)
find_package(nil-xalt CONFIG REQUIRED)
find_package(Lua REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(
    ${PROJECT_NAME} PUBLIC
    nil::xalt
    nil::clix
    ${LUA_LIBRARIES}
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    ${LUA_INCLUDE_DIR}
)