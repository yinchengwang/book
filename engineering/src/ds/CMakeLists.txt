# Trie 数据结构

include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_project_library(ds-trie DS_TRIE_SOURCES
)

# 创建测试（如果存在）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/trie_test.cpp")
    add_project_test(ds_trie_test DS_TRIE_TESTS ds-trie)
endif()
