# 算法练习源码目录

# 数据结构库
add_subdirectory(ds)

# 算法库（生产版）
add_subdirectory(algo-prod)

# 编程语言特性演示
add_subdirectory(cpp)

# LeetCode 题解（已迁移到 learning/code-solutions/，不再构建）
# add_subdirectory(leetcode)

# 面试题集合（已迁移到 learning/code-solutions/interview/，不再构建）
# add_subdirectory(interview)

# 可选模块（需要时取消注释）
# add_subdirectory(redis)    # Redis实现
# add_subdirectory(index)    # 向量索引（已迁移到 db/index/）
add_subdirectory(db)             # 数据库内核

# RAG 模块 (暂时禁用以便验证 db 重构)
# add_subdirectory(rag)            # RAG 系统

# kbase 库（轻量级推理引擎 + Obsidian 知识库）
add_subdirectory(kbase)
