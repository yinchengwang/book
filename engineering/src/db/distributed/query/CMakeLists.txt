# 分布式查询执行模块
# Phase E3: 分布式查询执行

file(GLOB_RECURSE DQ_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(DQ_SOURCE_FILES)
    add_library(db_distributed_query OBJECT ${DQ_SOURCE_FILES})
    target_include_directories(db_distributed_query PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/distributed
        ${ENGINEERING_SOURCE_DIR}/include/db/sharding
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
    target_link_libraries(db_distributed_query PRIVATE project_includes)

    # 链接依赖模块
    if(TARGET db_utils)
        target_link_libraries(db_distributed_query PRIVATE db_utils)
    endif()
    if(TARGET db_sharding)
        target_link_libraries(db_distributed_query PRIVATE db_sharding)
    endif()
    if(TARGET db_consensus)
        target_link_libraries(db_distributed_query PRIVATE db_consensus)
    endif()
endif()
