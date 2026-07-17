## 1. 基础改造 — 目录改名与 CMake 更新

- [ ] 1.1 创建 `engineering/apps/todo-app/` 目录，复制 `github-issue-todo/` 全部代码
- [ ] 1.2 更新 `engineering/apps/todo-app/CMakeLists.txt`：目标名从 `github-issue-todo` 改为 `todo-app`，路径更新
- [ ] 1.3 更新 `engineering/apps/CMakeLists.txt`：子目录路径指向新目录
- [ ] 1.4 删除旧的 `engineering/apps/github-issue-todo/` 目录
- [ ] 1.5 编译验证：`cmake --build build/engineering --target todo-app`

## 2. 数据模型扩展 — todo_model.c/h

- [ ] 2.1 `todo_model.h`：issue_t → todo_t 重命名，增加 priority、due_date、group_id、sort_order 字段
- [ ] 2.2 `todo_model.h`：status 扩展（open/closed/archived），issue_id → todo_id 统一重命名
- [ ] 2.3 `todo_model.h`：新增 group_t 结构体定义（id, name, color, sort_order, created_at）
- [ ] 2.4 `todo_model.h`：新增 comment_t 结构体定义（id, todo_id, text, created_at）
- [ ] 2.5 `todo_model.c`：全局状态变量重命名（g_issues → g_todos，g_issue_count → g_todo_count 等）
- [ ] 2.6 `todo_model.c`：JSON 序列化/反序列化支持新字段（priority, due_date, group_id, sort_order）
- [ ] 2.7 `todo_model.c`：JSON 序列化/反序列化新增 groups 和 comments 数据
- [ ] 2.8 `todo_model.c`：CRUD 函数更新（todo_create/todo_get_by_id/todo_update/todo_delete）
- [ ] 2.9 `todo_model.c`：issue_list → todo_list，增加 priority/group_id/due_before/due_after/sort 筛选
- [ ] 2.10 `todo_model.c`：checklist_list/checklist_add 等函数 issue_id → todo_id 重命名
- [ ] 2.11 `todo_model.c`：新增分组 CRUD（group_create/group_get/group_update/group_delete/group_list）
- [ ] 2.12 `todo_model.c`：新增评论 CRUD（comment_add/comment_list/comment_delete）
- [ ] 2.13 `todo_model.c`：新增 todo_update_sort 函数（更新 sort_order）
- [ ] 2.14 `todo_model.c`：新增 todo_get_stats 函数（统计看板数据）

## 3. 数据迁移 — todo_migration.c/h

- [ ] 3.1 创建 `todo_migration.h`：声明 `int todo_migrate_from_legacy(void)`
- [ ] 3.2 创建 `todo_migration.c`：检测旧文件 `issue_tool.json` 是否存在
- [ ] 3.3 `todo_migration.c`：读取旧 JSON 数据，添加默认字段值
- [ ] 3.4 `todo_migration.c`：写入新文件 `todo_app.json`，备份旧文件为 `.bak`
- [ ] 3.5 `todo_migration.c`：错误处理与回滚逻辑
- [ ] 3.6 `main.c`：启动时调用迁移函数

## 4. REST API 重写 — todo_handler.c/h

- [ ] 4.1 `todo_handler.h`：函数重命名（issue_handler → todo_handler）
- [ ] 4.2 `todo_handler.c`：路径路由 `/api/issues` → `/api/todos`
- [ ] 4.3 `todo_handler.c`：handle_list 增加优先级/分组/截止日期筛选参数
- [ ] 4.4 `todo_handler.c`：handle_create/handle_update 支持新字段
- [ ] 4.5 `todo_handler.c`：新增 handle_stats 处理 `GET /api/todos/stats`
- [ ] 4.6 `todo_handler.c`：新增 handle_sort 处理 `PATCH /api/todos/:id/sort`
- [ ] 4.7 `todo_handler.c`：新增评论处理（handle_comment_add/handle_comment_list/handle_comment_delete）
- [ ] 4.8 `todo_handler.c`：新增分组处理（handle_group_list/handle_group_create/handle_group_update/handle_group_delete）
- [ ] 4.9 `todo_handler.c`：静态文件服务路径更新（指向新的 web 目录）
- [ ] 4.10 `main.c`：更新打印信息（Todo App vs GitHub Issue Tool）

## 5. OPSX 变更桥接 — todo_change.c/h

- [ ] 5.1 创建 `todo_change.h`：声明 `int todo_create_change(int64_t todo_id, char *out_change_id, size_t out_size)`
- [ ] 5.2 创建 `todo_change.c`：实现子进程调用 `opsx propose` CLI
- [ ] 5.3 `todo_change.c`：错误处理（CLI 不可用时返回明确错误）
- [ ] 5.4 `todo_handler.c`：注册 `POST /api/todos/:id/create-change` 路由

## 6. 前端工程化 — Vue 3 + Vite 项目

- [ ] 6.1 初始化 `engineering/apps/web/todo-app/`：package.json + vite.config.js
- [ ] 6.2 创建 `index.html` 入口 + `src/main.js` Vue 应用初始化
- [ ] 6.3 配置 Vue Router（列表/看板/统计/分组管理路由）
- [ ] 6.4 创建 `src/api.js`：API 请求封装（CRUD + 新增端点）
- [ ] 6.5 创建 `App.vue` + `AppHeader.vue`：导航栏 + 搜索 + 新建按钮

## 7. 前端组件开发

- [ ] 7.1 `FilterBar.vue`：筛选面板（状态/优先级/分组/截止日期/搜索）
- [ ] 7.2 `TodoCard.vue`：待办卡片（标题/优先级/截止日期/标签/进度/过期高亮）
- [ ] 7.3 `ListView.vue`：列表视图（筛选 + 卡片列表 + 分页）
- [ ] 7.4 `BoardView.vue` + `BoardColumn.vue`：看板视图（按分组/优先级分组）
- [ ] 7.5 `StatsView.vue`：统计看板（图表/完成率/分布）
- [ ] 7.6 `GroupManager.vue`：分组管理（CRUD + 颜色选择）
- [ ] 7.7 `DetailPanel.vue`：详情侧边栏（编辑/Checklist/评论/OPSX 变更按钮）
- [ ] 7.8 `Checklist.vue`：待办清单组件
- [ ] 7.9 `Comments.vue`：评论组件
- [ ] 7.10 `CreateChangeBtn.vue`：OPSX 变更创建按钮
- [ ] 7.11 `CreateDialog.vue`：新建待办弹窗
- [ ] 7.12 `Pagination.vue`：分页组件
- [ ] 7.13 样式文件 `src/styles/main.css`：全局样式 + 暗色模式

## 8. 后端测试

- [ ] 8.1 `test_todo_model.cpp`：CRUD + 优先级 + 截止日期 + 分组 + 评论 + 排序测试
- [ ] 8.2 `test_todo_handler.cpp`：REST API 路由解析和响应测试
- [ ] 8.3 `test_todo_migration.cpp`：旧数据迁移兼容性测试
- [ ] 8.4 `CMakeLists.txt` 注册测试目标并验证编译通过

## 9. 集成联调

- [ ] 9.1 编译整个工程并修复编译错误
- [ ] 9.2 启动后端，curl 测试所有 API 端点
- [ ] 9.3 验证旧数据迁移（创建旧格式 JSON → 启动 → 检查新格式）
- [ ] 9.4 验证数据持久化（重启后数据不丢失）
- [ ] 9.5 前端构建产物可被后端正确 serve
- [ ] 9.6 端到端流程：创建待办 → 设置优先级/截止日期 → 添加 Checklist → 添加评论 → 创建 OPSX 变更