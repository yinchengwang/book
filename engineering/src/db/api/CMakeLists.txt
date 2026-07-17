# 向量 API 模块
file(GLOB_RECURSE API_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")
# 确保 vdb_api.c 被包含（GLOB_RECURSE 自动包含子目录所有 .c）

# uthash 头文件路径
find_path(UTHASH_INCLUDE_DIR uthash.h
    HINTS ${ENGINEERING_SOURCE_DIR}/../learning/include
    PATH_SUFFIXES uthash)

if(API_SOURCE_FILES)
    add_library(vector_api STATIC ${API_SOURCE_FILES})
    target_include_directories(vector_api PUBLIC
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/api
        ${ENGINEERING_SOURCE_DIR}/../third_part
        ${ENGINEERING_SOURCE_DIR}/../third_part/cjson
        ${UTHASH_INCLUDE_DIR}
    )
    target_link_libraries(vector_api PUBLIC
        db_core
        db_bm25
        project_includes
        cjson
    )
endif()
