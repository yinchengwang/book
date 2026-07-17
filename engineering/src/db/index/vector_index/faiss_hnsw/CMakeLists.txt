# faiss_hnsw 模块：内存版 HNSW 索引（参考 FAISS HNSW.cpp 重写）
# 独立静态库，便于其他模块链接

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

add_library(faiss_hnsw STATIC ${SRC})

target_include_directories(faiss_hnsw PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(faiss_hnsw PRIVATE algo-prod project_includes)