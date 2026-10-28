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
    vector_query.c  # Task 1: 从 core 迁入，消除 core 对 index/vector_index 的越界依赖
    vector_sql_functions_register.c  # Task 3: 向量模态 SQL 函数表
)

add_library(mm_vector STATIC ${VECTOR_PERSIST_SOURCES})

target_include_directories(mm_vector PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
)

# 链接必要的基础库
target_link_libraries(mm_vector PUBLIC
    mm_base  # Task 8: 独立化，db_core 依赖降为 mm_base（log/errors/rwlock）
    algo-prod
    mm_vector_index
    storage_mm
    storage_lock
    storage_wal  # P1-B Task 4：vector_wal 依赖 wal_flush_get_policy / db_fsync
    storage_page  # Task 4: vector_engine 依赖 disk_*；断 db_core->db_storage 环后不能再靠 aggregate 兜底
    db_vectorized  # Task 1: vector_query.c 依赖 vector_block_*/vector_batch_*（原在 db_core）
)
