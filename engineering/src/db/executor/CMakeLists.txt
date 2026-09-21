# executor 模块：执行层
# 依赖 parser, optimizer, storage, validator
add_subdirectory(operators)
add_subdirectory(framework)
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
    )
    target_link_libraries(db_executor PRIVATE db_parser db_optimizer db_storage db_validator)
    target_compile_options(db_executor PRIVATE -Wno-unused-parameter)
endif()

# RAG 执行器测试（Wave 3-⑧ 和 Wave 3-⑨）
# C9-3：rag/*.c 自包含（仅依赖 rag_executor.h / graphrag.h / vector_engine.h 与
# 标准库），直接编入测试二进制，绕开当前 bit-rot 的 db_executor 整库
# （header 冲突 + sqlExecutor API 漂移，归 Gap03 统一执行器变更处理）。
if(BUILD_TESTING)
    add_executable(test_rag_executor
        ${ENGINEERING_SOURCE_DIR}/test/db/executor/rag_executor_test.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rag/rag_executor.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rag/rag_pipeline.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rag/chunk_op.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rag/embed_op.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rag/retrieve_op.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rag/generate_op.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rag/graphrag_context.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rag/graphrag_entity.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rag/graphrag_hybrid_search.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rag/graphrag_relation.c
    )
    target_include_directories(test_rag_executor PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/executor
        ${THIRD_PART_DIR}/googletest/googletest/include
        ${THIRD_PART_DIR}/googletest/googlemock/include
    )
    target_link_libraries(test_rag_executor PRIVATE
        db_core
        project_includes
        gtest
        gtest_main
    )
    add_test(NAME rag_executor_test COMMAND test_rag_executor)
endif()
