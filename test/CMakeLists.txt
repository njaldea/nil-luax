project(xlua-test)

add_test_executable(
    ${PROJECT_NAME}
    global_fn.cpp
    global_variable.cpp
    custom_type_with_methods.cpp
    custom_type_with_properties.cpp
    custom_type_with_call_operator.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE xlua)
target_link_libraries(${PROJECT_NAME} PRIVATE GTest::gmock)
target_link_libraries(${PROJECT_NAME} PRIVATE GTest::gtest)
target_link_libraries(${PROJECT_NAME} PRIVATE GTest::gtest_main)