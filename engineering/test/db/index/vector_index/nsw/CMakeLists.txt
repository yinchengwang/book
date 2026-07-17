# NSW 索引测试 - 独立构建

file(GLOB NSW_TEST_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

add_executable(nsw_vector_test ${NSW_TEST_SOURCES})

target_include_directories(nsw_vector_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
)

target_link_libraries(nsw_vector_test PRIVATE
    nsw
    algo-prod
    heap_vector_store
    gtest
    gtest_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(nsw_vector_test
    PROPERTIES LABELS "nsw;vector_index"
)