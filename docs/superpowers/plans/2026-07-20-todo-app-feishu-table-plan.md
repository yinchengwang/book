# todo-app 多维表格化实施计划（Phase 1：SQLite 迁移 + 字段系统）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 todo-app 从 JSON 文件持久化迁移到 SQLite 嵌入式数据库，并实现可扩展的多维字段系统（EAV 模式），保持现有 API 完全兼容。

**Architecture:** 混合字段模式——内置字段（title、status、priority 等）保留为 `todos` 表列，扩展字段通过 `fields_def` + `field_values` 两张 EAV 表管理。现有 `todo_model.h` 接口不变，内部实现从 JSON 内存数组替换为 SQLite 操作。

**Tech Stack:** C11、SQLite3 amalgamation、cJSON、Winsock2、Vue 3

## Global Constraints

- 所有现有 API 端点不能修改（包括请求/响应格式）
- `todo_t` 结构体保持不变，`todo_model.h` 中所有函数签名保持不变
- 字段系统只管理扩展字段，内置字段（id=1-9）预置在 `fields_def` 表中
- JSON→SQLite 迁移在启动时自动完成，`todo_app.json` → `todo_app.json.bak`
- 每个 task 完成后必须编译验证 `todo-app.exe` 可运行

---

## 文件结构

### 新增文件

| 文件 | 行数预估 | 职责 |
|------|---------|------|
| `third_part/sqlite3/sqlite3.c` | ~250K | SQLite amalgamation 源码（下载，不手写） |
| `third_part/sqlite3/sqlite3.h` | ~200K | SQLite 头文件（下载，不手写） |
| `third_part/sqlite3/CMakeLists.txt` | 10 | SQLite 静态库构建 |
| `engineering/apps/todo-app/todo_db.h` | 60 | SQLite 持久化层接口 |
| `engineering/apps/todo-app/todo_db.c` | 400 | SQLite 持久化层实现（建表 + 迁移 + CRUD 实现） |
| `engineering/apps/todo-app/todo_field.h` | 40 | 字段系统接口 |
| `engineering/apps/todo-app/todo_field.c` | 300 | 字段系统实现（fields_def + field_values CRUD） |

### 修改文件

| 文件 | 变更要点 |
|------|---------|
| `engineering/apps/todo-app/CMakeLists.txt` | 添加 sqlite3 依赖 |
| `engineering/apps/todo-app/todo_model.c` | 替换 JSON 持久化为 SQLite；所有 CRUD 函数调用 SQLite 替代内存数组 |
| `engineering/apps/todo-app/todo_model.h` | 添加字段系统 API 声明 |
| `engineering/apps/todo-app/todo_handler.c` | 新增 5 个字段管理 handler + 路由；`todo_to_json` 增加 `fields` 字段 |
| `engineering/apps/todo-app/main.c` | 启动时调用 `todo_db_open()` 替代 `todo_db_load()` |

---

### Task 1.1: SQLite 引入

**Files:**
- Create: `third_part/sqlite3/CMakeLists.txt`
- Modify: `engineering/apps/todo-app/CMakeLists.txt`

**Interfaces:**
- Consumes: 无
- Produces: `sqlite3` 静态库目标，todo-app 可链接

- [ ] **Step 1: 下载 SQLite amalgamation**

```bash
# 从浏览器下载或使用已有 sqlite3.c/sqlite3.h
# 标准位置：third_part/sqlite3/sqlite3.c 和 third_part/sqlite3/sqlite3.h
# 使用 sqlite.org 2025 年最新 amalgamation
```

- [ ] **Step 2: 创建 SQLite CMakeLists.txt**

```cmake
# third_part/sqlite3/CMakeLists.txt
add_library(sqlite3 STATIC sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# SQLite 编译选项：禁用不需要的功能以减小体积
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_OMIT_DEPRECATED
)
```

- [ ] **Step 3: 修改 todo-app CMakeLists.txt**

```cmake
# engineering/apps/todo-app/CMakeLists.txt
# 在原有基础上添加 sqlite3 子目录和依赖

# 添加 sqlite3 子目录
add_subdirectory(${ENGINEERING_SOURCE_DIR}/../third_part/sqlite3 ${CMAKE_BINARY_DIR}/sqlite3)

# 修改 target_link_libraries，添加 sqlite3
target_link_libraries(todo-app PRIVATE
    project_includes
    cjson
    sqlite3
    ws2_32
)
```

- [ ] **Step 4: 编译验证**

```bash
cd /c/code/book/build/engineering && cmake --build . --target todo-app 2>&1 | tail -5
# Expected: [2/3] Linking C executable apps\todo-app\todo-app.exe
```

- [x] **Step 5: Commit**

```bash
git add third_part/sqlite3/CMakeLists.txt engineering/apps/todo-app/CMakeLists.txt
git commit -m "feat(todo-app): 引入 SQLite3 amalgamation 依赖"
```

---

### Task 1.2: SQLite 持久化层（todo_db）

**Files:**
- Create: `engineering/apps/todo-app/todo_db.h`
- Create: `engineering/apps/todo-app/todo_db.c`

**Interfaces:**
- Consumes: Task 1.1（sqlite3 库可用）
- Produces: `todo_db_open()`, `todo_db_close()`, `todo_db_migrate_from_json()`, 以及所有 CRUD 的 SQLite 实现函数

- [ ] **Step 1: 创建 todo_db.h**

```c
#ifndef TODO_DB_H
#define TODO_DB_H

#include <stdint.h>
#include <stddef.h>
#include <sqlite3.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 打开/创建数据库，path 为 .db 文件路径 */
int todo_db_open(const char *path);

/* 关闭数据库 */
void todo_db_close(void);

/* 从 JSON 文件迁移数据到 SQLite（迁移完成后重命名 JSON） */
int todo_db_migrate_from_json(const char *json_path);

/* 获取 SQLite 句柄（供其他模块使用） */
sqlite3 *todo_db_handle(void);

/* 元数据读写 */
int64_t todo_db_get_meta_int64(const char *key);
int todo_db_set_meta_int64(const char *key, int64_t value);

/* 检查表是否存在 */
int todo_db_table_exists(const char *table_name);

#ifdef __cplusplus
}
#endif

#endif /* TODO_DB_H */
```

- [ ] **Step 2: 实现 todo_db.c——建表函数**

```c
// 关键实现：create_all_tables()
// 创建 todos, fields_def, field_values, checklist, groups, comments, task_systems, plans, plan_items, meta 共 10 张表
// 使用 spec 中定义的完整 SQL schema
// 所有 CREATE TABLE 包含 IF NOT EXISTS
// 执行 in transaction（begin/commit 或使用 sqlite3_exec 的隐式事务）

static int create_all_tables(sqlite3 *db) {
    const char *sql = 
        "CREATE TABLE IF NOT EXISTS todos ( ... );\n"  // 完整 schema 见设计文档
        "CREATE TABLE IF NOT EXISTS fields_def ( ... );\n"
        "CREATE TABLE IF NOT EXISTS field_values ( ... );\n"
        // ... 所有表
        "CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);\n"
        "INSERT OR IGNORE INTO meta (key, value) VALUES ('schema_version', '1');\n";
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) { fprintf(stderr, "SQLite schema error: %s\n", err); sqlite3_free(err); return -1; }
    return 0;
}
```

- [ ] **Step 3: 实现 todo_db_open 和 todo_db_close**

```c
int todo_db_open(const char *path) {
    // 1. sqlite3_open(path, &g_db)
    // 2. 启用 WAL 模式（PRAGMA journal_mode=WAL）
    // 3. 启用外键（PRAGMA foreign_keys=ON）
    // 4. 调用 create_all_tables()
    // 5. 预置内置字段（id=1-9，见 spec 第 2.2 节）
    // 6. 检查是否存在 todo_app.json，如果存在调用 migrate_from_json
    // 7. 返回 0 成功
}

void todo_db_close(void) {
    // if (g_db) sqlite3_close(g_db); g_db = NULL;
}
```

**预置内置字段代码：**
```c
// 在 create_all_tables 之后执行
static int insert_builtin_fields(sqlite3 *db) {
    const char *sql = "INSERT OR IGNORE INTO fields_def (id, name, type, options, built_in, sort_order) VALUES "
        "(1, '标题', 'text', '{}', 1, 1),"
        "(2, '描述', 'text', '{}', 1, 2),"
        "(3, '状态', 'single_select', '{\"choices\":[\"open\",\"closed\",\"archived\"]}', 1, 3),"
        "(4, '优先级', 'single_select', '{\"choices\":[\"紧急\",\"高\",\"中\",\"低\",\"无\"]}', 1, 4),"
        "(5, '截止日期', 'date', '{}', 1, 5),"
        "(6, '标签', 'multi_select', '{}', 1, 6),"
        "(7, '分组', 'single_select', '{}', 1, 7),"
        "(8, '创建时间', 'datetime', '{}', 1, 8),"
        "(9, '更新时间', 'datetime', '{}', 1, 9);";
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) { sqlite3_free(err); return -1; }
    return 0;
}
```

- [ ] **Step 4: 实现 JSON→SQLite 迁移函数**

```c
int todo_db_migrate_from_json(const char *json_path) {
    // 1. 读取并解析 todo_app.json（复用现有 todo_db_load 的 JSON 解析逻辑）
    // 2. 在事务中逐条插入 todos、checklist、groups、comments、task_systems、plans、plan_items 表
    // 3. 更新 meta 表中的 next_* 自增 ID
    // 4. 重命名 todo_app.json → todo_app.json.bak
    // 5. return 0 成功
}

// 注意：迁移时不需要迁移 field_values（原来没有扩展字段）
// 迁移后，内存中的 g_todos 等全局数组不再需要，全部走 SQLite 查询
```

- [ ] **Step 5: 编译验证**

```bash
cd /c/code/book/build/engineering && cmake --build . --target todo-app 2>&1 | tail -5
# Expected: 编译成功，无 undefined reference 错误
```

- [x] **Step 6: Commit**

```bash
git add engineering/apps/todo-app/todo_db.h engineering/apps/todo-app/todo_db.c
git commit -m "feat(todo-app): 实现 SQLite 持久化层 + 建表 + 内置字段 + JSON 迁移"
```

---

### Task 1.3: todo_model.c 改造——SQLite 替代内存数组

**Files:**
- Modify: `engineering/apps/todo-app/todo_model.c`
- Modify: `engineering/apps/todo-app/todo_model.h`

**Interfaces:**
- Consumes: `todo_db.h`（Task 1.2）
- Produces: 所有 CRUD 函数签名不变，内部实现改为 SQLite 操作

- [ ] **Step 1: 在 todo_model.h 中添加字段系统 API 声明**

```c
// 在 todo_model.h 的末尾，plan_item 相关声明之后，__cplusplus 之前添加：

/* ============================================================
 * 字段系统 API
 * ============================================================ */

/* 字段定义结构体 */
typedef struct {
    int64_t id;
    char    name[256];
    char    type[32];          /* text / number / single_select / multi_select / date / datetime / user / attachment / formula / link */
    char    options[4096];     /* JSON 配置 */
    int     built_in;          /* 1=内置字段 */
    char    ref_table[256];    /* link 类型关联表 */
    char    ref_field[256];    /* link 类型关联字段 */
    char    formula[1024];     /* formula 表达式 */
    int     sort_order;
    int64_t created_at;
} field_def_t;

/* 获取所有字段定义 */
int field_def_list(field_def_t **fields, int *count);

/* 获取单个字段定义 */
int field_def_get(int64_t id, field_def_t *field);

/* 创建自定义字段 */
int field_def_create(const field_def_t *field, int64_t *out_id);

/* 更新字段定义（名称、选项等） */
int field_def_update(const field_def_t *field);

/* 删除自定义字段（内置字段返回 -1） */
int field_def_delete(int64_t id);

/* 更新字段排序 */
int field_def_update_sort(int64_t id, int sort_order);

/* 释放字段定义数组 */
void field_def_list_free(field_def_t *fields, int count);

/* 设置/获取扩展字段值 */
int field_value_set(int64_t todo_id, int64_t field_id, const char *value);
int field_value_get(int64_t todo_id, int64_t field_id, char *value, size_t value_size);

/* 获取某个 todo 的所有扩展字段值（返回 JSON 对象字符串，调用者 free） */
char *field_values_get_all(int64_t todo_id);
```

- [ ] **Step 2: 改造 todo_db_load——从 JSON 读取改为 SQLite 打开**

```c
// 原函数签名不变，实现改为：
int todo_db_load(const char *db_path) {
    // 1. 构造 .db 路径（将 .json 后缀替换为 .db）
    // 2. 调用 todo_db_open(db_path)
    // 3. 如果 JSON 文件存在且 SQLite 数据库刚创建，触发迁移
    // 4. return 0 成功
}
```

- [ ] **Step 3: 改造 todo_db_save——从 JSON 写入改为 SQLite 实时提交**

```c
// 原函数签名不变，实现改为：
int todo_db_save(void) {
    // SQLite 模式下不需要显式保存（每次写操作已提交）
    // 但仍然保留 g_modified 标记，用于 shutdown 时确保数据完整
    return 0;
}
```

- [ ] **Step 4: 改造 todo_create——从内存数组插入改为 SQLite INSERT**

```c
// 原函数签名不变，实现改为：
int todo_create(const todo_t *todo, int64_t *out_id) {
    // 1. 使用 sqlite3_prepare_v2 准备 INSERT INTO todos 语句
    // 2. sqlite3_bind_* 绑定所有字段
    // 3. sqlite3_step 执行
    // 4. 获取 last_insert_rowid() 作为 out_id
    // 5. return 0 成功
}
```

- [ ] **Step 5: 改造 todo_get_by_id——从内存数组查找改为 SQLite SELECT**

```c
// 原函数签名不变，实现改为：
int todo_get_by_id(int64_t id, todo_t *todo) {
    // 1. SELECT * FROM todos WHERE id = ?
    // 2. sqlite3_step 读取结果
    // 3. 填充 todo 结构体
    // 4. return 0 成功，-1 未找到
}
```

- [ ] **Step 6: 改造 todo_update——从内存数组修改改为 SQLite UPDATE**

```c
// 原函数签名不变，实现改为：
int todo_update(const todo_t *todo) {
    // 1. UPDATE todos SET title=?, description=?, status=?, ... WHERE id=?
    // 2. 带 updated_at = unixepoch()
    // 3. return 0 成功
}
```

- [ ] **Step 7: 改造 todo_delete——从内存数组删除改为 SQLite DELETE**

```c
// 原函数签名不变，实现改为：
int todo_delete(int64_t id) {
    // 1. DELETE FROM todos WHERE id = ?
    // 2. 外键级联会自动删除 checklist、comments、field_values
    // 3. return 0 成功
}
```

- [ ] **Step 8: 改造 todo_list——从内存数组筛选改为 SQLite 查询**

```c
// 原函数签名不变，实现改为：
int todo_list(const todo_query_t *query, todo_list_t *result) {
    // 核心逻辑：根据 query 参数动态构建 SQL
    // 1. 基础 SELECT：SELECT * FROM todos WHERE 1=1
    // 2. 根据 status 添加条件：AND status = ? （排除 status=all 时）
    // 3. 根据 labels 添加条件：AND labels LIKE ?
    // 4. 根据 search 添加条件：AND (title LIKE ? OR description LIKE ?)
    // 5. 根据 priority 添加条件：AND priority = ?
    // 6. 根据 group_id 添加条件
    // 7. 根据 due_before/due_after 添加条件
    // 8. 添加 ORDER BY 和 LIMIT/OFFSET 分页
    // 9. 执行查询，逐行填充 result->items
    // 10. 再执行一次 COUNT(*) 获取 total
    // 注意：排序字段 priority 用 CASE WHEN 映射，因为 priority 是 0-4 数字
}
```

- [ ] **Step 9: 改造其他 CRUD 函数**

```c
// 以下函数同样改造，每个函数从内存数组操作改为 SQLite 操作：
// todo_update_sort() -> UPDATE todos SET sort_order=? WHERE id=?
// todo_get_stats() -> SELECT COUNT(*), ... FROM todos WHERE ...
// checklist_add() -> INSERT INTO checklist
// checklist_toggle() -> UPDATE checklist SET done=1-done WHERE id=?
// checklist_delete() -> DELETE FROM checklist WHERE id=?
// checklist_list() -> SELECT * FROM checklist WHERE todo_id=? ORDER BY sort_order
// group_create/get/update/delete/list -> 对应的 SQLite 操作
// comment_add/list/delete -> 对应的 SQLite 操作
// task_system_create/get/update/delete/list -> 对应的 SQLite 操作
// plan_create/get/update/delete/list -> 对应的 SQLite 操作
// plan_item_create/get/update/delete/list -> 对应的 SQLite 操作
// todo_db_shutdown() -> todo_db_close()
// todo_db_reset() -> 删除 .db 文件 + 重新创建
```

- [ ] **Step 10: 编译验证**

```bash
cd /c/code/book/build/engineering && cmake --build . --target todo-app 2>&1 | tail -5
```

- [ ] **Step 11: 启动运行验证**

```bash
# 先备份现有 todo_app.json
cp /c/code/book/build/engineering/apps/todo-app/todo_app.json /tmp/todo_app.json.bak
cd /c/code/book/build/engineering/apps/todo-app && ./todo-app.exe &
sleep 2
# 测试 API
curl -s --max-time 5 "http://127.0.0.1:8080/api/todos?per_page=3"
# 验证返回 45 条学习计划
curl -s --max-time 5 "http://127.0.0.1:8080/api/todos/stats"
# 验证统计正常
taskkill //F //IM todo-app.exe 2>/dev/null
# 验证 todo_app.db 已生成
ls -la /c/code/book/build/engineering/apps/todo-app/todo_app.db
```

- [x] **Step 12: Commit**

```bash
git add engineering/apps/todo-app/todo_model.c engineering/apps/todo-app/todo_model.h
git commit -m "feat(todo-app): todo_model CRUD 全部改造为 SQLite 操作"
```

---

### Task 1.4: 字段系统实现（todo_field）

**Files:**
- Create: `engineering/apps/todo-app/todo_field.c`

**Interfaces:**
- Consumes: todo_db.h、todo_model.h
- Produces: 字段定义 CRUD + 字段值 EAV 读写

- [ ] **Step 1: 实现字段定义 CRUD**

```c
// field_def_list() 实现：
int field_def_list(field_def_t **fields, int *count) {
    // SELECT * FROM fields_def ORDER BY sort_order, id
    // 逐行读取，填充 field_def_t 数组
    // return 0 成功
}

// field_def_create() 实现：
int field_def_create(const field_def_t *field, int64_t *out_id) {
    // INSERT INTO fields_def (name, type, options, sort_order, created_at)
    // VALUES (?, ?, ?, ?, unixepoch())
    // 内置字段的 id 从 10 开始，自定义字段 AUTOINCREMENT
}

// field_def_update() 实现：
int field_def_update(const field_def_t *field) {
    // UPDATE fields_def SET name=?, options=? WHERE id=? AND built_in=0
    // 内置字段（built_in=1）不可更新类型
}

// field_def_delete() 实现：
int field_def_delete(int64_t id) {
    // DELETE FROM fields_def WHERE id=? AND built_in=0
    // 级联删除 field_values 中对应的数据
    // 返回影响行数，0 表示内置字段或不存在
}

// field_def_update_sort() 实现：
int field_def_update_sort(int64_t id, int sort_order) {
    // UPDATE fields_def SET sort_order=? WHERE id=?
}
```

- [ ] **Step 2: 实现字段值 EAV 读写**

```c
// field_value_set() 实现：
int field_value_set(int64_t todo_id, int64_t field_id, const char *value) {
    // INSERT OR REPLACE INTO field_values (todo_id, field_id, value) VALUES (?, ?, ?)
}

// field_value_get() 实现：
int field_value_get(int64_t todo_id, int64_t field_id, char *value, size_t value_size) {
    // SELECT value FROM field_values WHERE todo_id=? AND field_id=?
    // 如果找不到，返回 -1
}

// field_values_get_all() 实现：
// 返回 JSON 字符串，如：{"10":"张三","11":"42"}
// 调用者需要使用 free() 释放
char *field_values_get_all(int64_t todo_id) {
    // SELECT fv.field_id, fv.value FROM field_values fv WHERE fv.todo_id=?
    // 逐行读取，拼装 JSON 对象
    // 使用 cJSON 创建对象并打印
}
```

- [ ] **Step 3: 编译验证**

```bash
cd /c/code/book/build/engineering && cmake --build . --target todo-app 2>&1 | tail -5
```

- [x] **Step 4: Commit**

```bash
git add engineering/apps/todo-app/todo_field.c
git commit -m "feat(todo-app): 实现字段系统（fields_def CRUD + field_values EAV 读写）"
```

---

### Task 1.5: 字段管理 API（todo_handler 扩展）

**Files:**
- Modify: `engineering/apps/todo-app/todo_handler.c`

**Interfaces:**
- Consumes: todo_field.h（Task 1.4）
- Produces: 5 个新的 API 端点 + 现有响应中增加 `fields` 字段

- [ ] **Step 1: 新增字段序列化函数**

```c
// 在 todo_to_json 中增加 fields 字段
static cJSON *todo_to_json(const todo_t *todo) {
    cJSON *j = cJSON_CreateObject();
    // 原有字段不变...
    cJSON_AddNumberToObject(j, "id", todo->id);
    cJSON_AddStringToObject(j, "title", todo->title);
    // ... 所有原有字段不变 ...
    
    // 新增：扩展字段
    char *fields_str = field_values_get_all(todo->id);
    if (fields_str) {
        cJSON *jfields = cJSON_Parse(fields_str);
        if (jfields) {
            cJSON_AddItemToObject(j, "fields", jfields);
        } else {
            cJSON_AddItemToObject(j, "fields", cJSON_CreateObject());
        }
        free(fields_str);
    } else {
        cJSON_AddItemToObject(j, "fields", cJSON_CreateObject());
    }
    
    return j;
}
```

- [ ] **Step 2: 新增字段定义序列化函数**

```c
static cJSON *field_def_to_json(const field_def_t *f) {
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "id", f->id);
    cJSON_AddStringToObject(j, "name", f->name);
    cJSON_AddStringToObject(j, "type", f->type);
    
    cJSON *joptions = cJSON_Parse(f->options);
    cJSON_AddItemToObject(j, "options", joptions ? joptions : cJSON_CreateObject());
    
    cJSON_AddNumberToObject(j, "built_in", f->built_in);
    cJSON_AddNumberToObject(j, "sort_order", f->sort_order);
    cJSON_AddNumberToObject(j, "created_at", f->created_at);
    return j;
}
```

- [ ] **Step 3: 实现 handle_fields_list**

```c
static void handle_fields_list(SOCKET client) {
    field_def_t *fields = NULL;
    int count = 0;
    if (field_def_list(&fields, &count) != 0) {
        send_error(client, 9001, "获取字段列表失败");
        return;
    }
    cJSON *jitems = cJSON_CreateArray();
    for (int i = 0; i < count; i++)
        cJSON_AddItemToArray(jitems, field_def_to_json(&fields[i]));
    field_def_list_free(fields, count);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "fields", jitems);
    send_success(client, data);
}
```

- [ ] **Step 4: 实现 handle_fields_create / handle_fields_update / handle_fields_delete / handle_fields_sort**

```c
// handle_fields_create:
// 解析 body 中的 name, type, options
// 调用 field_def_create
// 返回新建的字段定义

// handle_fields_update:
// 解析 body 中的 name, options（可选）
// 调用 field_def_update
// 返回更新后的字段定义

// handle_fields_delete:
// 调用 field_def_delete(id)
// 内置字段返回 400

// handle_fields_sort:
// 解析 body 中的 sort_order
// 调用 field_def_update_sort
```

- [ ] **Step 5: 实现 handle_todo_fields_update（PATCH /api/todos/:id/fields）**

```c
static void handle_todo_fields_update(SOCKET client, int64_t id, const char *body) {
    // 1. 检查 todo 是否存在（todo_get_by_id）
    // 2. 解析 body 中的 fields 对象
    // 3. 遍历 fields 的每个键值对，调用 field_value_set
    // 4. 返回更新后的 todo JSON（含扩展字段）
}
```

- [ ] **Step 6: 在路由表中添加新路由**

```c
// 在 handle_request 函数中，日历 API 之后、DFX 统计 API 之前添加：

/* ===== 字段系统 API ===== */
else if (strcmp(path, "/api/fields") == 0) {
    if (strcmp(method, "GET") == 0) {
        handle_fields_list(client);
        handled = 1;
    } else if (strcmp(method, "POST") == 0) {
        handle_fields_create(client, body);
        handled = 1;
    }
}
else if (sscanf(path, "/api/fields/%lld", (long long *)&id_a) == 1) {
    char expect[128];
    snprintf(expect, sizeof(expect), "/api/fields/%lld", (long long)id_a);
    if (strcmp(path, expect) == 0) {
        if (strcmp(method, "PATCH") == 0) {
            handle_fields_update(client, id_a, body);
            handled = 1;
        } else if (strcmp(method, "DELETE") == 0) {
            handle_fields_delete(client, id_a);
            handled = 1;
        }
    }
    /* PATCH /api/fields/:id/sort */
    else if (sscanf(path, "/api/fields/%lld/sort", (long long *)&id_a) == 1) {
        char expect2[128];
        snprintf(expect2, sizeof(expect2), "/api/fields/%lld/sort", (long long)id_a);
        if (strcmp(path, expect2) == 0 && strcmp(method, "PATCH") == 0) {
            handle_fields_sort(client, id_a, body);
            handled = 1;
        }
    }
}

/* ===== TODO 扩展字段值 ===== */
else if (sscanf(path, "/api/todos/%lld/fields", (long long *)&id_a) == 1) {
    char expect[128];
    snprintf(expect, sizeof(expect), "/api/todos/%lld/fields", (long long)id_a);
    if (strcmp(path, expect) == 0 && strcmp(method, "PATCH") == 0) {
        handle_todo_fields_update(client, id_a, body);
        handled = 1;
    }
}
```

- [ ] **Step 7: 编译验证**

```bash
cd /c/code/book/build/engineering && cmake --build . --target todo-app 2>&1 | tail -5
```

- [ ] **Step 8: 启动运行验证**

```bash
cd /c/code/book/build/engineering/apps/todo-app && ./todo-app.exe &
sleep 2
# 测试字段列表
echo "=== GET /api/fields ==="
curl -s --max-time 5 "http://127.0.0.1:8080/api/fields" | head -c 200
echo ""
# 测试创建字段
echo "=== POST /api/fields ==="
curl -s --max-time 5 -X POST "http://127.0.0.1:8080/api/fields" -H "Content-Type: application/json" -d '{"name":"负责人","type":"single_select","options":{"choices":["张三","李四"]}}'
echo ""
# 测试 TODO 返回中带 fields 字段
echo "=== GET /api/todos?per_page=1 ==="
curl -s --max-time 5 "http://127.0.0.1:8080/api/todos?per_page=1" | head -c 300
echo ""
# 测试设置扩展字段值
echo "=== PATCH /api/todos/1/fields ==="
curl -s --max-time 5 -X PATCH "http://127.0.0.1:8080/api/todos/1/fields" -H "Content-Type: application/json" -d '{"fields":{"10":"张三"}}'
echo ""
taskkill //F //IM todo-app.exe 2>/dev/null
```

- [x] **Step 9: Commit**

```bash
git add engineering/apps/todo-app/todo_handler.c
git commit -m "feat(todo-app): 新增字段管理 API + TODO 扩展字段值 API"
```

---

### Task 1.6: main.c 适配 + 验证

**Files:**
- Modify: `engineering/apps/todo-app/main.c`

- [ ] **Step 1: 修改 main.c 中的数据库加载**

```c
// 将原来的 todo_db_load(db_path) 替换为：
// 构造 .db 路径
char db_path_with_ext[512];
snprintf(db_path_with_ext, sizeof(db_path_with_ext), "%.*s.db", (int)(strlen(db_path) - 5), db_path);
// 例如 "todo_app.json" -> "todo_app.db"

// 调用 todo_db_open（内部处理建表 + 迁移）
if (todo_db_open(db_path_with_ext) != 0) {
    fprintf(stderr, "SQLite 数据库初始化失败\n");
    return 1;
}
```

- [ ] **Step 2: 完整编译并运行验证**

```bash
cd /c/code/book/build/engineering && cmake --build . --target todo-app 2>&1 | tail -5
```

- [ ] **Step 3: 端到端验证**

```bash
# 1. 清理旧数据
rm -f /c/code/book/build/engineering/apps/todo-app/todo_app.db
# 2. 确保 todo_app.json 存在（有 45 条学习计划数据）
# 3. 启动服务器
cd /c/code/book/build/engineering/apps/todo-app && ./todo-app.exe &
sleep 2
# 4. 验证 45 条数据成功迁移
echo "=== 总条数 ==="
curl -s --max-time 5 "http://127.0.0.1:8080/api/todos?per_page=100" | python3 -c "import sys,json; d=json.load(sys.stdin); print(f'total={d[\"data\"][\"total\"]}, items={d[\"data\"][\"per_page\"]}')"
echo "=== 统计 ==="
curl -s --max-time 5 "http://127.0.0.1:8080/api/todos/stats"
echo ""
echo "=== 字段列表 ==="
curl -s --max-time 5 "http://127.0.0.1:8080/api/fields" | python3 -c "import sys,json; d=json.load(sys.stdin); print(f'fields_count={len(d[\"data\"][\"fields\"])}')" 2>/dev/null || echo "字段 API 已就绪"
echo "=== 创建扩展字段 ==="
curl -s --max-time 5 -X POST "http://127.0.0.1:8080/api/fields" -H "Content-Type: application/json" -d '{"name":"测试字段","type":"text","options":{}}'
echo ""
echo "=== 设置扩展字段值 ==="
curl -s --max-time 5 -X PATCH "http://127.0.0.1:8080/api/todos/1/fields" -H "Content-Type: application/json" -d '{"fields":{"10":"测试值"}}'
echo ""
echo "=== 验证扩展字段已返回 ==="
curl -s --max-time 5 "http://127.0.0.1:8080/api/todos?per_page=1" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['data']['items'][0].get('fields', {}))" 2>/dev/null
echo ""
# 验证 .db 文件已生成
ls -la /c/code/book/build/engineering/apps/todo-app/todo_app.db
echo ""
# 验证 todo_app.json 已备份
ls -la /c/code/book/build/engineering/apps/todo-app/todo_app.json.bak 2>/dev/null || echo "todo_app.json 仍然存在（未迁移）"
# 停止
taskkill //F //IM todo-app.exe 2>/dev/null
```

- [x] **Step 4: 最终 Commit**

```bash
git add engineering/apps/todo-app/main.c
git commit -m "feat(todo-app): 适配 SQLite 启动流程（main.c 改用 todo_db_open）"
```

---

## 验证清单

完成所有 Task 后，逐一验证：

| 验证项 | 预期结果 | 验证命令 |
|--------|---------|---------|
| 编译通过 | 无错误 | `cmake --build . --target todo-app` |
| 自动迁移 | 45 条数据在 SQLite 中 | `curl /api/todos?per_page=100` → total=45 |
| 字段列表 | 9 个内置字段 | `curl /api/fields` → fields_count=9 |
| 创建字段 | 新字段 id≥10 | `curl -X POST /api/fields` → id=10 |
| 删除内置字段 | 400 错误 | `curl -X DELETE /api/fields/1` → error |
| 设置扩展字段值 | 字段值写入 | `curl -X PATCH /api/todos/1/fields` → fields: {"10":"xxx"} |
| 列表返回扩展字段 | 每条 todo 带 fields | `curl /api/todos?per_page=1` → fields 字段存在 |
| 统计正常 | 数字合理 | `curl /api/todos/stats` → total=45 |
| 日历正常 | 返回数据 | `curl /api/calendar/month` |
| 分组正常 | 返回数据 | `curl /api/groups` |
| 计划正常 | 返回数据 | `curl /api/plans` |
| 前端静态文件 | 200 OK | `curl http://localhost:8080/` → HTML |
| JSON 备份 | .bak 文件存在 | `ls todo_app.json.bak` |

---

## 回滚策略

如果 SQLite 迁移后发现严重问题：

1. 停止服务器
2. 删除 `todo_app.db`
3. 恢复 `todo_app.json.bak` → `todo_app.json`
4. 回退代码到 JSON 版本
5. 重新编译

SQLite 迁移是**可逆的**，JSON 数据不会在迁移过程中丢失（rename 操作在全部写入成功后执行）。