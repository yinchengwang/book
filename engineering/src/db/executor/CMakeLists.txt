# executor 模块：执行层
# 依赖 parser, optimizer, storage, validator
file(GLOB_RECURSE EXECUTOR_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(EXECUTOR_SOURCE_FILES)
    add_library(db_executor STATIC ${EXECUTOR_SOURCE_FILES})
    target_include_directories(db_executor PUBLIC
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/executor
        ${ENGINEERING_SOURCE_DIR}/include/db/parser
        ${ENGINEERING_SOURCE_DIR}/include/db/optimizer
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/validator
        ${ENGINEERING_SOURCE_DIR}/include/db/core
        ${ENGINEERING_SOURCE_DIR}/include/db/executor/vector
        ${ENGINEERING_SOURCE_DIR}/include/db/executor/graph
        ${ENGINEERING_SOURCE_DIR}/include/db/executor/kv
        ${ENGINEERING_SOURCE_DIR}/include/db/executor/ts
        ${ENGINEERING_SOURCE_DIR}/include/db/executor/doc
        ${ENGINEERING_SOURCE_DIR}/include/db/executor/spatial
        ${ENGINEERING_SOURCE_DIR}/include/db/executor/datalog
        ${ENGINEERING_SOURCE_DIR}/include/db/executor/yang
        ${ENGINEERING_SOURCE_DIR}/include/db/executor/stream
    )
    target_link_libraries(db_executor PRIVATE db_parser db_optimizer db_storage db_validator sql_engine)
    target_compile_options(db_executor PRIVATE -Wno-unused-parameter)
endif()

# RAG 执行器测试（Wave 3-⑧ 和 Wave 3-⑨）
if(BUILD_TESTING)
    add_executable(test_rag_executor
        ${ENGINEERING_SOURCE_DIR}/test/db/executor/rag_executor_test.cpp
    )
    target_include_directories(test_rag_executor PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/executor
        ${THIRD_PART_DIR}/googletest/googletest/include
        ${THIRD_PART_DIR}/googletest/googlemock/include
    )
    target_link_libraries(test_rag_executor PRIVATE
        db_executor
        db_core
        project_includes
        gtest
        gtest_main
    )
    add_test(NAME rag_executor_test COMMAND test_rag_executor)
endif()
