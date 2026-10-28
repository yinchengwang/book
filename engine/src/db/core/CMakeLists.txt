# ========================================================================
# mm_base（L0 基础设施）+ db_core（查询/服务侧核心）
#
# Task 6 分层拆分：
#   mm_base  —— errors/log/guc/metrics/structured_log/读写锁/序列化契约/
#              sha256。只依赖 multimodal_config_gen + Threads，
#              不依赖 db_storage / db_core / 任何模态。
#   db_core  —— initdb/pg_ctl/db_server/func_registry/查询服务等，
#              PUBLIC 链接 mm_storage_api（含 mm_base，依赖只向下）。
#   engine_registry.c 已迁至 mm_storage_api（见 storage/mm/CMakeLists.txt）。
# ========================================================================

add_library(mm_base STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/errors.c
    ${CMAKE_CURRENT_SOURCE_DIR}/error_codes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/guc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/structured_log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/metrics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mm_record.c         # C0-3 序列化契约
    ${CMAKE_CURRENT_SOURCE_DIR}/sha256.c            # Blob SHA-256 / Manifest 校验
    ${ENGINEERING_SOURCE_DIR}/src/db/common_rwlock.c         # 读写锁：kv/vector/graph 引擎使用
    ${ENGINEERING_SOURCE_DIR}/src/db/concurrency/mmdb_lock.c # canonical 读写锁（C9-2）
)
target_include_directories(mm_base PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/core
)
target_link_libraries(mm_base PUBLIC multimodal_config_gen Threads::Threads)
target_compile_options(mm_base PRIVATE -Wno-unused-parameter)
set_target_properties(mm_base PROPERTIES FOLDER "Libraries")

add_library(db_core STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/initdb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pg_ctl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/db_server.c
    # func_registry.c 于 Task 7 迁入 mm_storage_api（SQL 函数注册表属 L1，
    # 模态注册文件引用 func_registry_register，冒烟闭包不应含 db_core）
    ${CMAKE_CURRENT_SOURCE_DIR}/query_filter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/query_fusion.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mvcc_session.c      # C2-1 MVCC 线程局部
    ${CMAKE_CURRENT_SOURCE_DIR}/xml_parser.c        # C2-5 自研 XML 解析器
    # cjk_tokenizer.c 于 Task 12 迁入 storage/doc（中文分词只服务文档模态）
    ${CMAKE_CURRENT_SOURCE_DIR}/explain_analyze.c   # C2-2 EXPLAIN/ANALYZE
    ${CMAKE_CURRENT_SOURCE_DIR}/mm_storage_blob.c   # C3-1 T10: mm_storage 接入 Blob 类型
)
target_include_directories(db_core PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/core
)
# L1 公共层链接在 src/db/CMakeLists.txt 中 add_subdirectory(storage) 之后声明
# （mm_storage_api 目标彼时才存在，link 名在生成期才解析，此处提前写会退化为 -l 名）。
# engine_registry.h 公开包含 db/multimodal_config.h，生成头必须随 db_core 传递
target_link_libraries(db_core PUBLIC multimodal_config_gen)
target_compile_options(db_core PRIVATE -Wno-unused-parameter)

# Windows 需要链接 ws2_32（db_server）
if(WIN32)
    target_link_libraries(db_core PRIVATE ws2_32)
endif()
