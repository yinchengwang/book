# 独立应用程序

# 公共代码
add_subdirectory(common)

# 游戏
add_subdirectory(games)

# 工具
add_subdirectory(tools)

# VDB CLI 工具
add_subdirectory(vdb_cli)

# Web 项目（静态页面，无需编译）
# add_subdirectory(web)  # reading-radar 和 knowledge_hub 是静态项目

# Todo App 待办事项应用
add_subdirectory(todo-app)

# kbase CLI 工具 - 已迁移到 apps/multimodal_rag (Phase 10 清理)
# add_subdirectory(kbase)

# 多模态 RAG 服务（迁移自 rag/ + kbase/，统一后端）
add_subdirectory(multimodal_rag)