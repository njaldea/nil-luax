project(xlua-test)

add_test_executable(${PROJECT_NAME} test.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE xlua)
target_link_libraries(${PROJECT_NAME} PRIVATE GTest::gmock)
target_link_libraries(${PROJECT_NAME} PRIVATE GTest::gtest)
target_link_libraries(${PROJECT_NAME} PRIVATE GTest::gtest_main)