# Gap#2 向量化执行引擎测试
enable_testing()

add_executable(vectorized_exec_test
    block_test.cpp
    simd_test.cpp
    filter_test.cpp
    agg_test.cpp
    hashagg_test.cpp
    hashjoin_test.cpp
)
target_link_libraries(vectorized_exec_test
    db_vectorized
    db_core
    project_includes
    gtest
    gtest_main
)
target_include_directories(vectorized_exec_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/vectorized
)

add_test(NAME vectorized_exec_test COMMAND vectorized_exec_test)

# 同一台机器上分别验证三层内核（AVX2 / SSE2 / 纯标量）结果逐位一致。
# MMDB_SIMD 只能把分派降级，不会启用硬件不支持的指令集，因此在任何 CPU 上都安全：
# 若硬件本就不支持某级别，对应用例只是重复跑更低的内核，不会执行非法指令。
foreach(_simd_level sse4.2 sse2 scalar)
    add_test(NAME vectorized_exec_test_${_simd_level}
             COMMAND vectorized_exec_test)
    set_tests_properties(vectorized_exec_test_${_simd_level} PROPERTIES
        ENVIRONMENT "MMDB_SIMD=${_simd_level}")
endforeach()
