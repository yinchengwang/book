# 索引子系统
add_subdirectory(shared)

# 索引 MVCC 可见性判断（索引与 MVCC 集成）
if(TARGET index)
    target_sources(index PRIVATE
        index_mvcc.c
    )
endif()

# Hilbert 曲线索引
add_subdirectory(hilbert)

# 树索引
add_subdirectory(art)
add_subdirectory(bplus_tree)
add_subdirectory(btree)
add_subdirectory(radix_tree)
add_subdirectory(rtree)
add_subdirectory(skip_list)
add_subdirectory(ttree)

# Hash 索引
add_subdirectory(hash)

# 倒排索引
add_subdirectory(bitmap)
add_subdirectory(gin)
add_subdirectory(gist)
add_subdirectory(fulltext)

# 块索引
add_subdirectory(brin)

# 存储后端抽象层
add_subdirectory(storage_backend)

# 向量 Heap 主存储（Single Source of Truth）
add_subdirectory(heap)

# 向量索引
add_subdirectory(vector_index)
