# 存储后端抽象层
# 为多模态数据库索引子系统提供统一的存储接口实现

include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

# 公共函数库（destroy, create, type_name）
add_project_library(storage_backend_common STORAGE_BACKEND_COMMON_SOURCES
    storage_backend_common.c
)

target_include_directories(storage_backend_common
    PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include/db/index
)

# 纯内存存储后端（不持久化）
add_project_library(storage_backend_memory STORAGE_BACKEND_MEMORY_SOURCES
)

# storage_backend.h 位于 include/db/index/，需要显式添加搜索路径
target_include_directories(storage_backend_memory
    PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include/db/index
)
target_link_libraries(storage_backend_memory
    PRIVATE
        storage_backend_common
)

# 页面文件存储后端
add_project_library(storage_backend_pagefile PAGEFILE_SOURCES
    storage_page_file.c
)

target_include_directories(storage_backend_pagefile
    PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/index
)
target_link_libraries(storage_backend_pagefile
    PRIVATE
        storage_backend_common
)

# mmap 存储后端（POSIX mmap / Windows MapViewOfFile）
add_project_library(storage_backend_mmap MMAP_SOURCES
    storage_mmap.c
)

target_include_directories(storage_backend_mmap
    PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/index
)
target_link_libraries(storage_backend_mmap
    PRIVATE
        storage_backend_common
)
