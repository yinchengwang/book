# 执行器测试 - 向量扫描算子
if(BUILD_TESTING)
    add_executable(test_vector_scan
        ${ENGINEERING_SOURCE_DIR}/test/db/executor/vector_scan_test.cpp
    )
    target_include_directories(test_vector_scan PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/executor
        ${ENGINEERING_SOURCE_DIR}/include/db/executor/vector
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${THIRD_PART_DIR}/googletest/googletest/include
        ${THIRD_PART_DIR}/googletest/googlemock/include
    )
    target_link_libraries(test_vector_scan PRIVATE
        db_executor
        db_core
        vector_persist
        heap_vector_store
        storage_backend_common
        project_includes
        gtest
        gtest_main
    )
    add_test(NAME vector_scan_test COMMAND test_vector_scan)

    # RAG 执行器测试 - 测试已集成到 src/db/executor/CMakeLists.txt
endif()
