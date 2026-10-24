# 集成测试 CMake 配置

add_project_test(integration_test INTEGRATION_TEST_SRCS)
target_link_libraries(integration_test
    PRIVATE
        project_includes
        vector_index
        diskann
        ivf
        heap_vector_store
        storage_backend_common
        algo-prod
)

# db_utils 的源文件可能为空（utils/ 目前只有 CMakeLists.txt），
# 此时 add_library 不会执行、目标不存在，直接链接会导致配置期报错。
if(TARGET db_utils)
    target_link_libraries(integration_test PRIVATE db_utils)
endif()
