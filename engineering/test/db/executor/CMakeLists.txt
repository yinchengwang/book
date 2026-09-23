# engineering/test/db/executor/CMakeLists.txt
add_executable(executor_framework_test
    framework_test.cpp
)
target_link_libraries(executor_framework_test
    PRIVATE db_executor db_vectorized gtest gtest_main
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
    PRIVATE db_executor db_vectorized gtest gtest_main
)
target_include_directories(executor_integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(executor_integration_test)
add_test(NAME executor_integration_test COMMAND executor_integration_test)

add_executable(px_queue_test px_queue_test.cpp)
target_link_libraries(px_queue_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(px_queue_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(px_queue_test)

add_executable(px_scheduler_test px_scheduler_test.cpp)
target_link_libraries(px_scheduler_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(px_scheduler_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(px_scheduler_test)

add_executable(exchange_exec_test exchange_exec_test.cpp)
target_link_libraries(exchange_exec_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(exchange_exec_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(exchange_exec_test)

add_executable(plan_parallel_test plan_parallel_test.cpp)
target_link_libraries(plan_parallel_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(plan_parallel_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(plan_parallel_test)

add_executable(hashjoin_px_test hashjoin_px_test.cpp)
target_link_libraries(hashjoin_px_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(hashjoin_px_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(hashjoin_px_test)

add_executable(shard_prune_test shard_prune_test.cpp)
target_link_libraries(shard_prune_test PRIVATE db_executor db_vectorized db_sharding gtest gtest_main)
target_include_directories(shard_prune_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(shard_prune_test)

add_executable(px_wire_test px_wire_test.cpp)
target_link_libraries(px_wire_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(px_wire_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(px_wire_test)

add_executable(rpc_stream_test rpc_stream_test.cpp)
target_link_libraries(rpc_stream_test PRIVATE db_executor db_vectorized db_rpc gtest gtest_main)
target_include_directories(rpc_stream_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(rpc_stream_test)

add_executable(px_benchmark_test px_benchmark_test.cpp)
target_link_libraries(px_benchmark_test PRIVATE db_executor db_vectorized gtest gtest_main)
target_include_directories(px_benchmark_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(px_benchmark_test)
