# 向量 API 模块
file(GLOB_RECURSE API_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")
# 确保 vdb_api.c 被包含（GLOB_RECURSE 自动包含子目录所有 .c）

# uthash 头文件路径
# 原先跨轨引用 learning 轨的头文件目录；learning 切出独立仓后改为工程内的
# third_part/uthash/src。该目录直接存放 uthash.h，故不能保留
# PATH_SUFFIXES uthash（那会去 src/uthash/ 下找，而该子目录不存在）。
find_path(UTHASH_INCLUDE_DIR uthash.h
    HINTS ${ENGINEERING_SOURCE_DIR}/third_part/uthash/src)

if(API_SOURCE_FILES)
    add_library(vector_api STATIC ${API_SOURCE_FILES})
    target_include_directories(vector_api PUBLIC
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/api
        ${ENGINEERING_SOURCE_DIR}/../third_part
        ${ENGINEERING_SOURCE_DIR}/third_part/cjson
        ${UTHASH_INCLUDE_DIR}
    )
    target_link_libraries(vector_api PUBLIC
        db_core
        db_bm25
        project_includes
        cjson
        vector_persist  # P1-B Task 4：vector_wal_open/close/append 符号来源
    )
endif()
