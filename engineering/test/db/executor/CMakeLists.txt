# engineering/test/db/executor/CMakeLists.txt
add_executable(executor_framework_test
    framework_test.cpp
)
target_link_libraries(executor_framework_test
    PRIVATE db_executor db_vectorized gtest
)
target_include_directories(executor_framework_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(executor_framework_test)
add_test(NAME executor_framework_test COMMAND executor_framework_test)

add_executable(executor_integration_test
    integration_test.cpp
)
target_link_libraries(executor_integration_test
    PRIVATE db_executor db_vectorized gtest
)
target_include_directories(executor_integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(executor_integration_test)
add_test(NAME executor_integration_test COMMAND executor_integration_test)
