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

    # KV 扫描算子测试
    add_executable(test_kv_scan
        ${ENGINEERING_SOURCE_DIR}/test/db/executor/kv_scan_test.cpp
    )
    target_include_directories(test_kv_scan PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/executor
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${THIRD_PART_DIR}/googletest/googletest/include
        ${THIRD_PART_DIR}/googletest/googlemock/include
    )
    target_link_libraries(test_kv_scan PRIVATE
        db_storage
        project_includes
        gtest
        gtest_main
    )
    add_test(NAME kv_scan_test COMMAND test_kv_scan)

    # 时序扫描算子测试
    # 注意：ts_engine 默认禁用，测试需要手动启用 ts 模块
    # 启用方式：取消 src/db/storage/CMakeLists.txt 中 add_subdirectory(ts) 的注释
    if(TARGET storage_ts)
        add_executable(test_ts_scan
            ${ENGINEERING_SOURCE_DIR}/test/db/executor/ts_scan_test.cpp
        )
        target_include_directories(test_ts_scan PRIVATE
            ${ENGINEERING_SOURCE_DIR}/include
            ${ENGINEERING_SOURCE_DIR}/include/db
            ${ENGINEERING_SOURCE_DIR}/include/db/executor
            ${ENGINEERING_SOURCE_DIR}/include/db/executor/ts
            ${ENGINEERING_SOURCE_DIR}/include/db/storage
            ${THIRD_PART_DIR}/googletest/googletest/include
            ${THIRD_PART_DIR}/googletest/googlemock/include
        )
        target_link_libraries(test_ts_scan PRIVATE
            db_executor
            db_core
            $<TARGET_OBJECTS:storage_ts>
            project_includes
            gtest
            gtest_main
        )
        add_test(NAME ts_scan_test COMMAND test_ts_scan)
    endif()

    # RAG 执行器测试 - 测试已集成到 src/db/executor/CMakeLists.txt

    # 空间扫描算子测试
    add_executable(test_spatial_scan
        ${ENGINEERING_SOURCE_DIR}/test/db/executor/spatial_scan_test.cpp
    )
    target_include_directories(test_spatial_scan PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/executor
        ${ENGINEERING_SOURCE_DIR}/include/db/executor/spatial
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${THIRD_PART_DIR}/googletest/googletest/include
        ${THIRD_PART_DIR}/googletest/googlemock/include
    )
    target_link_libraries(test_spatial_scan PRIVATE
        db_executor
        db_core
        storage_spatial
        project_includes
        gtest
        gtest_main
    )
    add_test(NAME spatial_scan_test COMMAND test_spatial_scan)

    # 文档扫描算子测试
    # 注意：doc_engine 默认禁用，测试需要手动启用 doc 模块
    add_executable(test_doc_scan
        ${ENGINEERING_SOURCE_DIR}/test/db/executor/doc_scan_test.cpp
    )
    target_include_directories(test_doc_scan PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/executor
        ${ENGINEERING_SOURCE_DIR}/include/db/executor/doc
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${THIRD_PART_DIR}/googletest/googletest/include
        ${THIRD_PART_DIR}/googletest/googlemock/include
    )
    target_link_libraries(test_doc_scan PRIVATE
        db_executor
        db_core
        project_includes
        gtest
        gtest_main
    )
    add_test(NAME doc_scan_test COMMAND test_doc_scan)

    # 图扫描算子测试
    add_executable(test_graph_scan
        ${ENGINEERING_SOURCE_DIR}/test/db/executor/graph_scan_test.cpp
    )
    target_include_directories(test_graph_scan PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/executor
        ${ENGINEERING_SOURCE_DIR}/include/db/executor/graph
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/graph
        ${THIRD_PART_DIR}/googletest/googletest/include
        ${THIRD_PART_DIR}/googletest/googlemock/include
    )
    target_link_libraries(test_graph_scan PRIVATE
        db_executor
        db_core
        db_storage
        project_includes
        gtest
        gtest_main
    )
    add_test(NAME graph_scan_test COMMAND test_graph_scan)

    # Yang 扫描算子测试
    if(TARGET storage_yang)
        add_executable(test_yang_scan
            ${ENGINEERING_SOURCE_DIR}/test/db/executor/yang_scan_test.cpp
        )
        target_include_directories(test_yang_scan PRIVATE
            ${ENGINEERING_SOURCE_DIR}/include
            ${ENGINEERING_SOURCE_DIR}/include/db
            ${ENGINEERING_SOURCE_DIR}/include/db/executor
            ${ENGINEERING_SOURCE_DIR}/include/db/executor/yang
            ${ENGINEERING_SOURCE_DIR}/include/db/storage
            ${ENGINEERING_SOURCE_DIR}/include/db/core
            ${THIRD_PART_DIR}/googletest/googletest/include
            ${THIRD_PART_DIR}/googletest/googlemock/include
        )
        target_link_libraries(test_yang_scan PRIVATE
            db_executor
            db_core
            $<TARGET_OBJECTS:storage_yang>
            project_includes
            gtest
            gtest_main
        )
        add_test(NAME yang_scan_test COMMAND test_yang_scan)
    endif()

    # Datalog 扫描算子测试
    add_executable(test_datalog_scan
        ${ENGINEERING_SOURCE_DIR}/test/db/executor/datalog_scan_test.cpp
    )
    target_include_directories(test_datalog_scan PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/executor
        ${ENGINEERING_SOURCE_DIR}/include/db/executor/datalog
        ${THIRD_PART_DIR}/googletest/googletest/include
        ${THIRD_PART_DIR}/googletest/googlemock/include
    )
    target_link_libraries(test_datalog_scan PRIVATE
        db_executor
        db_core
        project_includes
        gtest
        gtest_main
    )
    add_test(NAME datalog_scan_test COMMAND test_datalog_scan)

    # 流处理算子测试
    add_executable(test_stream_scan
        ${ENGINEERING_SOURCE_DIR}/test/db/executor/stream_scan_test.cpp
    )
    target_include_directories(test_stream_scan PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/executor
        ${ENGINEERING_SOURCE_DIR}/include/db/executor/stream
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${THIRD_PART_DIR}/googletest/googletest/include
        ${THIRD_PART_DIR}/googletest/googlemock/include
    )
    target_link_libraries(test_stream_scan PRIVATE
        db_executor
        db_core
        project_includes
        gtest
        gtest_main
    )
    add_test(NAME stream_scan_test COMMAND test_stream_scan)
endif()
