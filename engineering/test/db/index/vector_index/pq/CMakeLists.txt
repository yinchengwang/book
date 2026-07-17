# PQ 量化器测试

file(GLOB PQ_TEST_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

add_executable(pq_test ${PQ_TEST_SOURCES})

target_include_directories(pq_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
)

target_link_libraries(pq_test PRIVATE
    pq
    gtest
    gtest_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(pq_test
    PROPERTIES LABELS "pq;quantization"
)