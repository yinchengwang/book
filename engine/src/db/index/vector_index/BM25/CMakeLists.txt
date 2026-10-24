# BM25 向量索引模块
file(GLOB_RECURSE BM25_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(BM25_SOURCE_FILES)
    add_library(db_bm25 STATIC ${BM25_SOURCE_FILES})
    target_include_directories(db_bm25 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
    )
    target_link_libraries(db_bm25 PUBLIC algo-prod)
endif()
