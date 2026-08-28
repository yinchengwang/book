# core 模块：核心基础设施（errors, log, guc, initdb, pg_ctl, db_server, structured_log）
add_library(db_core STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/errors.c
    ${CMAKE_CURRENT_SOURCE_DIR}/log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/guc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/initdb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pg_ctl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/db_server.c
    ${CMAKE_CURRENT_SOURCE_DIR}/vector_query.c
    ${CMAKE_CURRENT_SOURCE_DIR}/vector_exec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/structured_log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/metrics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/query_filter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/query_fusion.c
    ${CMAKE_CURRENT_SOURCE_DIR}/engine_registry.c  # 引擎注册
    ${CMAKE_CURRENT_SOURCE_DIR}/mm_record.c         # C0-3 序列化契约
    ${CMAKE_CURRENT_SOURCE_DIR}/mvcc_session.c      # C2-1 MVCC 线程局部
    ${CMAKE_CURRENT_SOURCE_DIR}/xml_parser.c        # C2-5 自研 XML 解析器
    ${CMAKE_CURRENT_SOURCE_DIR}/cjk_tokenizer.c     # C2-6 中文分词 + 词干化
    ${CMAKE_CURRENT_SOURCE_DIR}/explain_analyze.c   # C2-2 EXPLAIN/ANALYZE
    ${CMAKE_CURRENT_SOURCE_DIR}/sha256.c            # Blob SHA-256: 实现 Manifest 编解码与完整性校验（Task 3）)
)
target_include_directories(db_core PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/core
)
target_compile_options(db_core PRIVATE -Wno-unused-parameter)

# Windows 需要链接 ws2_32
if(WIN32)
    target_link_libraries(db_core PRIVATE ws2_32)
endif()
