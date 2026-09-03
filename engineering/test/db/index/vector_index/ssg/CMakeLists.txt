# SSG 索引测试 - 独立构建

file(GLOB SSG_TEST_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

add_executable(ssg_vector_test ${SSG_TEST_SOURCES})

target_include_directories(ssg_vector_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
)

target_link_libraries(ssg_vector_test PRIVATE
    ssg
    algo-prod
    heap_vector_store
    vector_index
    storage_backend_memory
    db_storage
    gtest
    gtest_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(ssg_vector_test
    PROPERTIES LABELS "ssg;vector_index"
)