# IVF 模块 CMakeLists.txt
# IVF 倒排文件索引实现

# 源文件列表
set(IVF_SOURCES
    faiss_direct_map.c
    faiss_inverted_lists.c
    faiss_ivf_add.c
    faiss_ivf_core.c
    faiss_ivf_search.c
    faiss_ivf_train.c
    faiss_ivf_utils.c
)

# 创建静态库
add_library(ivf STATIC ${IVF_SOURCES})

# 设置 include 目录
target_include_directories(ivf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

# 链接依赖
target_link_libraries(ivf PRIVATE
    self_made_cpp
    project_includes
    Threads::Threads
)
