# 向量持久化层
# src/db/storage/vector/CMakeLists.txt

# 独立的向量持久化库（基于段的持久化方案）
# 这是一套新的持久化接口，与现有的 vector_engine.c 是独立实现的

set(VECTOR_PERSIST_SOURCES
    vector_page.c
    vector_segment.c
    vector_index_persist.c
    vector_wal.c
    checkpoint_manager.c  # P1-B Task 7：后台 CheckpointManager
    vector_buf.c
    vector_engine.c
    vec_page.c
)

add_library(vector_persist STATIC ${VECTOR_PERSIST_SOURCES})

target_include_directories(vector_persist PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
)

# 链接必要的基础库
target_link_libraries(vector_persist PUBLIC
    db_core
    algo-prod
    vector_index
    storage_mm
    storage_lock
    storage_wal  # P1-B Task 4：vector_wal 依赖 wal_flush_get_policy / db_fsync
)
