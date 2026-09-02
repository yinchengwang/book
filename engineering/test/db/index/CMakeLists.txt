# 索引子系统测试

add_subdirectory(heap)
add_subdirectory(storage_backend)
add_subdirectory(integration)
add_subdirectory(sift_test)
add_subdirectory(vector_index)  # 向量索引测试（包含 GPU 索引）

# 索引框架单元测试 (IndexCatalog, IndexManager, IndexCost, IndexSelector)
add_executable(index_framework_test index_manager_test.cpp)

target_include_directories(index_framework_test PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_part/googletest/googletest/include
)

target_link_libraries(index_framework_test PRIVATE
    index_framework
    gtest
    gtest_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(index_framework_test
    PROPERTIES LABELS "index;framework"
)