# 代码答案集合（LeetCode 题解 + 面试题解 + 分布式）
# 学习层独立构建入口

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/c/CMakeLists.txt")
    add_subdirectory(c)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/interview/CMakeLists.txt")
    add_subdirectory(interview)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/distributed/CMakeLists.txt")
    add_subdirectory(distributed)
endif()