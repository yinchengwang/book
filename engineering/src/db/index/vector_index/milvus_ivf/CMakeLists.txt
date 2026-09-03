# Milvus IVF 风格索引库

add_library(milvus_ivf STATIC milvus_ivf.c)

target_include_directories(milvus_ivf PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(milvus_ivf PRIVATE project_includes Threads::Threads)