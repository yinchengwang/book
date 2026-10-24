# IVF-PQ 索引测试

add_executable(ivf_pq_test ivf_pq_test.cpp)

target_include_directories(ivf_pq_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../..
    ${ENGINEERING_SOURCE_DIR}/src/db/index/vector_index
)

target_link_libraries(ivf_pq_test PRIVATE
    vector_index
    algo-prod
    gtest
    gtest_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(ivf_pq_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES LABELS "vector_index;ivf_pq"
)