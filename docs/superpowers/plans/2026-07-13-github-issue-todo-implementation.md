# GitHub Issue 待办工具 — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**目标：** 实现一个 Web 版 GitHub Issues 风格待办工具，使用项目自有关系存储引擎作为数据后端

**架构：** C 后端（libmicrohttpd HTTP 服务器 + 关系引擎表操作）+ Vue 3 前端 SPA

**技术栈：** C11, libmicrohttpd (third_part), 项目自有关系引擎 (table.h/table.c), cjson, Vue 3 (CDN 模式), Vite

## 全局约束

- 所有 C 代码使用 C11 标准
- 前端代码放在 `engineering/apps/web/github-issue-todo/`
- 后端代码放在 `engineering/apps/github-issue-todo/`
- 数据存储使用项目自有关系引擎（`table.h` API）
- HTTP 服务器使用 `third_part/libmicrohttpd`
- JSON 序列化使用项目自带的 cjson
- 统一响应格式：`{"code": 0, "data": ..., "msg": "ok"}`
- Issue 表名：`issues`，Checklist 表名：`checklist_items`

---

### 任务 1：创建后端目录结构和 CMakeLists.txt

**文件：**
- 创建：`engineering/apps/github-issue-todo/CMakeLists.txt`
- 创建：`engineering/apps/github-issue-todo/main.c`（仅骨架）
- 创建：`engineering/apps/github-issue-todo/issue_model.h`
- 创建：`engineering/apps/github-issue-todo/issue_model.c`（仅骨架）
- 创建：`engineering/apps/github-issue-todo/issue_handler.h`
- 创建：`engineering/apps/github-issue-todo/issue_handler.c`（仅骨架）
- 修改：`engineering/apps/CMakeLists.txt`（添加 add_subdirectory）

**接口：**
- 无（基础搭建）

- [ ] **步骤 1：创建 CMakeLists.txt**

```cmake
# GitHub Issue 待办工具
# Web 版 Issue 管理 + 关系引擎存储

# 源文件
file(GLOB ISSUE_TOOL_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "*.c"
)

# 创建可执行文件
add_executable(github-issue-todo ${ISSUE_TOOL_SOURCES})

# 包含目录
target_include_directories(github-issue-todo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/parser
    ${ENGINEERING_SOURCE_DIR}/include/db/executor
    ${ENGINEERING_SOURCE_DIR}/include/db/storage
    ${ENGINEERING_SOURCE_DIR}/../third_part
)

# 链接库
target_link_libraries(github-issue-todo PRIVATE
    db_core
    project_includes
)

# 链接第三方库
target_link_libraries(github-issue-todo PRIVATE
    ${ENGINEERING_SOURCE_DIR}/../third_part/libmicrohttpd/lib/libmicrohttpd.a
    ${ENGINEERING_SOURCE_DIR}/../third_part/cjson/libcjson.a
)
```

- [ ] **步骤 2：创建 issue_model.h（头文件骨架）**

```c
#ifndef ISSUE_MODEL_H
#define ISSUE_MODEL_H

#include "db/table.h"
#include "db/kv.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

#define ISSUE_TITLE_MAX       256
#define ISSUE_DESC_MAX        4096
#define ISSUE_STATUS_MAX      16
#define ISSUE_LABELS_MAX      512
#define CHECKLIST_TEXT_MAX    256

/* ============================================================
 * 数据结构
 * ============================================================ */

typedef struct {
    int64_t id;
    char    title[ISSUE_TITLE_MAX];
    char    description[ISSUE_DESC_MAX];
    char    status[ISSUE_STATUS_MAX];      /* "open" / "closed" */
    char    labels[ISSUE_LABELS_MAX];      /* JSON 数组字符串 */
    int64_t created_at;
    int64_t updated_at;
} issue_t;

typedef struct {
    int64_t id;
    int64_t issue_id;
    char    text[CHECKLIST_TEXT_MAX];
    int     done;          /* 0 / 1 */
    int     sort_order;
} checklist_item_t;

/* 查询参数 */
typedef struct {
    const char *status;     /* NULL = 全部 */
    const char *labels;     /* NULL = 全部, 逗号分隔 */
    const char *search;     /* NULL = 不搜索 */
    int         page;
    int         per_page;
} issue_query_t;

typedef struct {
    issue_t *items;
    int      count;
    int      total;
} issue_list_t;

/* ============================================================
 * 数据库生命周期
 * ============================================================ */

/**
 * @brief 初始化数据库并创建表
 * @param db KV 数据库句柄
 * @return 0 成功，-1 失败
 */
int issue_db_init(kv_t *db);

/**
 * @brief 关闭数据库
 */
void issue_db_shutdown(void);

/* ============================================================
 * Issue CRUD
 * ============================================================ */

int issue_create(kv_t *db, const issue_t *issue, int64_t *out_id);
int issue_get_by_id(kv_t *db, int64_t id, issue_t *issue);
int issue_update(kv_t *db, const issue_t *issue);
int issue_delete(kv_t *db, int64_t id);
int issue_list(kv_t *db, const issue_query_t *query, issue_list_t *result);
void issue_list_free(issue_list_t *result);

/* ============================================================
 * Checklist 操作
 * ============================================================ */

int checklist_add(kv_t *db, int64_t issue_id, const char *text, checklist_item_t *item);
int checklist_toggle(kv_t *db, int64_t item_id);
int checklist_delete(kv_t *db, int64_t item_id);
int checklist_list(kv_t *db, int64_t issue_id, checklist_item_t **items, int *count);
void checklist_list_free(checklist_item_t *items, int count);

#ifdef __cplusplus
}
#endif

#endif /* ISSUE_MODEL_H */
```

- [ ] **步骤 3：创建其他骨架文件（空函数体）**

```c
// main.c — 仅含 main 函数打印 "not implemented yet"
#include <stdio.h>
int main(void) {
    printf("github-issue-todo: not implemented yet\n");
    return 0;
}
```

issue_model.c 和 issue_handler.c 创建空文件，只包含 `#include` 和函数骨架。

- [ ] **步骤 4：修改父 CMakeLists.txt**

在 `engineering/apps/CMakeLists.txt` 末尾添加：
```cmake
# GitHub Issue 待办工具
add_subdirectory(github-issue-todo)
```

- [ ] **步骤 5：编译验证**

```bash
cd c:/code/book && cmake --build build/engineering --target github-issue-todo 2>&1 | tail -20
```
预期：编译成功，生成 `build/engineering/apps/github-issue-todo/github-issue-todo.exe`

- [ ] **步骤 6：提交**

```bash
git add engineering/apps/CMakeLists.txt engineering/apps/github-issue-todo/
git commit -m "feat(github-issue-todo): 搭建后端目录结构和 CMake 编译"
```

---

### 任务 2：实现数据模型层 — issue_model.c

**文件：**
- 修改：`engineering/apps/github-issue-todo/issue_model.c`
- 修改：`engineering/apps/github-issue-todo/issue_model.h`（如需补充）

**接口：**
- 消费：`kv_t *` (来自 `db/kv.h`)
- 生产：`issue_db_init`, `issue_create`, `issue_get_by_id`, `issue_update`, `issue_delete`, `issue_list`, `checklist_add`, `checklist_toggle`, `checklist_delete`, `checklist_list`

**表结构：**

issues 表定义：
| 列 | 类型 | 备注 |
|---|---|---|
| id | SQL_TYPE_INT | 主键 |
| title | SQL_TYPE_VARCHAR(256) | NOT NULL |
| description | SQL_TYPE_TEXT | |
| status | SQL_TYPE_VARCHAR(16) | DEFAULT 'open' |
| labels | SQL_TYPE_VARCHAR(512) | JSON 数组 |
| created_at | SQL_TYPE_INT | |
| updated_at | SQL_TYPE_INT | |

checklist_items 表定义：
| 列 | 类型 | 备注 |
|---|---|---|
| id | SQL_TYPE_INT | 主键 |
| issue_id | SQL_TYPE_INT | NOT NULL |
| text | SQL_TYPE_VARCHAR(256) | NOT NULL |
| done | SQL_TYPE_INT | DEFAULT 0 |
| sort_order | SQL_TYPE_INT | DEFAULT 0 |

- [ ] **步骤 1：实现 issue_db_init**

```c
int issue_db_init(kv_t *db) {
    // 1. 检查表是否已存在
    table_t *existing = table_open(db, "issues");
    if (existing) {
        table_close(existing);
        return 0;  // 表已存在，无需重复创建
    }

    // 2. 创建 issues 表
    table_column_t issue_cols[] = {
        {"id",          SQL_TYPE_INT,      0, true,  true},
        {"title",       SQL_TYPE_VARCHAR,  256, true,  false},
        {"description", SQL_TYPE_TEXT,     0,   false, false},
        {"status",      SQL_TYPE_VARCHAR,  16,  false, false},
        {"labels",      SQL_TYPE_VARCHAR,  512, false, false},
        {"created_at",  SQL_TYPE_INT,      0,   false, false},
        {"updated_at",  SQL_TYPE_INT,      0,   false, false},
    };
    table_t *issues = table_create(db, "issues", issue_cols, 7);
    if (!issues) return -1;
    table_close(issues);

    // 3. 创建 checklist_items 表
    table_column_t check_cols[] = {
        {"id",        SQL_TYPE_INT,      0, true,  true},
        {"issue_id",  SQL_TYPE_INT,      0, false, false},
        {"text",      SQL_TYPE_VARCHAR,  256, true,  false},
        {"done",      SQL_TYPE_INT,      0,   false, false},
        {"sort_order", SQL_TYPE_INT,     0,   false, false},
    };
    table_t *checklist = table_create(db, "checklist_items", check_cols, 5);
    if (!checklist) return -1;
    table_close(checklist);

    return 0;
}
```

- [ ] **步骤 2：实现 issue_create**

```c
int issue_create(kv_t *db, const issue_t *issue, int64_t *out_id) {
    table_t *table = table_open(db, "issues");
    if (!table) return -1;

    // 构造值数组：id(title, description, status, labels, created_at, updated_at)
    // id 为自增，传入 NULL 让引擎自动分配
    int64_t now = (int64_t)time(NULL);
    const char *status = issue->status[0] ? issue->status : "open";

    const void *values[] = {
        NULL,                          // id 自增
        issue->title,
        issue->description,
        status,
        issue->labels,
        &now,
        &now
    };

    int ret = table_insert(table, values, 7);
    if (ret == 0 && out_id) {
        // 获取最后插入的 ID（需要查看 table.h 是否提供此能力）
        // 如果 table_insert 不返回 ID，可以通过扫描获取最大 ID
        // 这里先返回 0，后续完善
        *out_id = 0;
    }

    table_close(table);
    return ret;
}
```

**注意：** `table_insert` 返回值需要确认是否返回行 ID。如果它不返回自增 ID，需要在 `table.h` 中补充 `table_last_insert_rowid()` 函数，或通过其他方式获取。具体实现时根据实际情况调整。

- [ ] **步骤 3：实现 issue_get_by_id**

```c
int issue_get_by_id(kv_t *db, int64_t id, issue_t *issue) {
    table_t *table = table_open(db, "issues");
    if (!table) return -1;

    void *row = NULL;
    int ret = table_get_row(table, (uint64_t)id, &row);
    if (ret != 0) {
        table_close(table);
        return -1;
    }

    // 解析行数据到 issue_t
    // 行数据的布局由 table.h 的 table_row_t 结构决定
    // 需要根据实际存储格式解析
    // 这里先做骨架，具体解析在实现时完善

    free(row);
    table_close(table);
    return 0;
}
```

**注意：** 需要先了解 `table_get_row` 返回的 `row` 数据格式，再填充解析逻辑。

- [ ] **步骤 4：实现 issue_list**

```c
int issue_list(kv_t *db, const issue_query_t *query, issue_list_t *result) {
    table_t *table = table_open(db, "issues");
    if (!table) return -1;

    // 使用 table_scan 全表扫描
    table_iter_t *iter = table_scan(table);
    if (!iter) {
        table_close(table);
        return -1;
    }

    // 临时存储所有匹配行
    issue_t *matches = NULL;
    int count = 0, capacity = 0;

    while (table_iter_next(iter) == 0) {
        void *row = NULL;
        if (table_iter_get_row(iter, &row) != 0) continue;

        // 解析行数据
        issue_t issue;
        memset(&issue, 0, sizeof(issue));
        // TODO: 解析 row 到 issue（实现时根据 table_row_t 格式填充）

        // 筛选：按状态
        if (query->status && strcmp(query->status, "all") != 0) {
            if (strcmp(issue.status, query->status) != 0) {
                free(row);
                continue;
            }
        }

        // 筛选：按标签
        if (query->labels && query->labels[0]) {
            // 检查 issue 的标签是否包含所有查询标签
            // 需要解析 labels JSON 数组
            // TODO: 实现标签匹配
        }

        // 筛选：按关键词搜索
        if (query->search && query->search[0]) {
            if (strstr(issue.title, query->search) == NULL &&
                strstr(issue.description, query->search) == NULL) {
                free(row);
                continue;
            }
        }

        // 添加到结果
        if (count >= capacity) {
            capacity = capacity ? capacity * 2 : 16;
            issue_t *tmp = realloc(matches, capacity * sizeof(issue_t));
            if (!tmp) { free(row); break; }
            matches = tmp;
        }
        matches[count++] = issue;
        free(row);
    }

    table_iter_free(iter);
    table_close(table);

    // 分页
    result->total = count;
    int start = (query->page - 1) * query->per_page;
    if (start >= count) {
        result->items = NULL;
        result->count = 0;
    } else {
        int end = start + query->per_page;
        if (end > count) end = count;
        result->count = end - start;
        result->items = malloc(result->count * sizeof(issue_t));
        memcpy(result->items, matches + start, result->count * sizeof(issue_t));
    }

    free(matches);
    return 0;
}
```

- [ ] **步骤 5：实现 Checklist 操作**

checklist_add / checklist_toggle / checklist_delete / checklist_list 的实现模式与 issue 类似，使用 `checklist_items` 表。

- [ ] **步骤 6：编写单元测试**

在 `engineering/test/db/` 下创建 `test_issue_model.cpp`：

```cpp
#include <gtest/gtest.h>
extern "C" {
#include "db/kv.h"
#include "apps/github-issue-todo/issue_model.h"
}

class IssueModelTest : public ::testing::Test {
protected:
    kv_t *db;
    void SetUp() override {
        db = kv_open(":memory:");  // 使用内存数据库
        ASSERT_NE(db, nullptr);
        ASSERT_EQ(issue_db_init(db), 0);
    }
    void TearDown() override {
        kv_close(db);
    }
};

TEST_F(IssueModelTest, CreateAndGetIssue) {
    issue_t issue = {0};
    strcpy(issue.title, "测试 Issue");
    strcpy(issue.description, "这是一个测试");
    strcpy(issue.labels, "[\"bug\"]");

    int64_t id;
    ASSERT_EQ(issue_create(db, &issue, &id), 0);

    issue_t got;
    ASSERT_EQ(issue_get_by_id(db, id, &got), 0);
    ASSERT_STREQ(got.title, "测试 Issue");
}

TEST_F(IssueModelTest, ListWithFilter) {
    // 创建多个 Issue
    issue_t i1 = {0}; strcpy(i1.title, "Bug A"); strcpy(i1.labels, "[\"bug\"]");
    issue_t i2 = {0}; strcpy(i2.title, "Feature B"); strcpy(i2.labels, "[\"enhancement\"]");
    int64_t id1, id2;
    issue_create(db, &i1, &id1);
    issue_create(db, &i2, &id2);

    // 按状态筛选
    issue_query_t q = {"open", NULL, NULL, 1, 20};
    issue_list_t result;
    ASSERT_EQ(issue_list(db, &q, &result), 0);
    ASSERT_EQ(result.total, 2);
    issue_list_free(&result);

    // 按标签筛选
    q.labels = "bug";
    ASSERT_EQ(issue_list(db, &q, &result), 0);
    ASSERT_EQ(result.total, 1);
    issue_list_free(&result);
}

TEST_F(IssueModelTest, ChecklistOperations) {
    int64_t issue_id;
    issue_t issue = {0};
    strcpy(issue.title, "带 Checklist 的 Issue");
    issue_create(db, &issue, &issue_id);

    checklist_item_t item;
    ASSERT_EQ(checklist_add(db, issue_id, "第一步", &item), 0);
    ASSERT_EQ(checklist_add(db, issue_id, "第二步", &item), 0);

    checklist_item_t *items;
    int count;
    ASSERT_EQ(checklist_list(db, issue_id, &items, &count), 0);
    ASSERT_EQ(count, 2);
    checklist_list_free(items, count);
}
```

- [ ] **步骤 7：运行测试验证**

```bash
cd c:/code/book && ctest --test-dir build/engineering -R test_issue_model --output-on-failure
```
预期：所有测试 PASS

- [ ] **步骤 8：提交**

```bash
git add engineering/apps/github-issue-todo/issue_model.c engineering/apps/github-issue-todo/issue_model.h
git add engineering/test/db/test_issue_model.cpp
git commit -m "feat(github-issue-todo): 实现数据模型层 issue_model CRUD + 筛选"
```

---

### 任务 3：实现 HTTP 服务器 + REST API 路由

**文件：**
- 修改：`engineering/apps/github-issue-todo/main.c`
- 修改：`engineering/apps/github-issue-todo/issue_handler.c`
- 修改：`engineering/apps/github-issue-todo/issue_handler.h`

**接口：**
- 消费：`issue_model.h` 的所有函数
- 生产：REST API 端点处理函数

- [ ] **步骤 1：实现 issue_handler.h**

```c
#ifndef ISSUE_HANDLER_H
#define ISSUE_HANDLER_H

#include "db/kv.h"
#include <microhttpd.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 Issue 处理器
 * @param db KV 数据库句柄
 */
void issue_handler_init(kv_t *db);

/**
 * @brief libmicrohttpd 请求处理回调
 */
enum MHD_Result issue_handler(void *cls,
    struct MHD_Connection *connection,
    const char *url, const char *method,
    const char *version, const char *upload_data,
    size_t *upload_data_size, void **con_cls);

#ifdef __cplusplus
}
#endif

#endif /* ISSUE_HANDLER_H */
```

- [ ] **步骤 2：实现 issue_handler.c — JSON 工具函数**

```c
#include "issue_handler.h"
#include "issue_model.h"
#include "db/kv.h"
#include "cjson/cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* 发送 JSON 响应 */
static enum MHD_Result send_json(struct MHD_Connection *connection,
                                  int code, cJSON *data, const char *msg) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "code", code);
    cJSON_AddItemToObject(root, "data", data ? data : cJSON_CreateNull());
    cJSON_AddStringToObject(root, "msg", msg ? msg : "ok");

    char *str = cJSON_PrintUnformatted(root);
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(str), (void*)str, MHD_RESPMEM_MUST_COPY);
    MHD_add_response_header(response, "Content-Type", "application/json");
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");

    enum MHD_Result ret = MHD_queue_response(connection, code == 0 ? 200 : 400, response);
    MHD_destroy_response(response);
    free(str);
    cJSON_Delete(root);
    return ret;
}

/* 发送错误响应 */
static enum MHD_Result send_error(struct MHD_Connection *connection,
                                   int code, const char *msg) {
    return send_json(connection, code, NULL, msg);
}

/* 发送成功响应 */
static enum MHD_Result send_success(struct MHD_Connection *connection, cJSON *data) {
    return send_json(connection, 0, data, "ok");
}
```

- [ ] **步骤 3：实现路由分发**

```c
enum MHD_Result issue_handler(void *cls,
    struct MHD_Connection *connection,
    const char *url, const char *method,
    const char *version, const char *upload_data,
    size_t *upload_data_size, void **con_cls) {

    // 解析 URL 路径
    // /api/issues              → GET=列表, POST=创建
    // /api/issues/123          → GET=详情, PATCH=更新, DELETE=删除
    // /api/issues/123/checklist       → POST=添加待办
    // /api/issues/123/checklist/456   → PATCH=切换状态, DELETE=删除待办

    // 根据 method + URL 模式分发到具体处理函数
    // 使用 sscanf 解析 URL 中的 ID 参数
    // 使用 MHD_lookup_connection_value 获取查询参数
    // 对于 POST/PATCH，读取请求体（upload_data 回调）
}
```

- [ ] **步骤 4：实现 handle_list（GET /api/issues）**

解析查询参数 `status`, `labels`, `search`, `page`, `per_page`，调用 `issue_list()`，将结果序列化为 JSON。

- [ ] **步骤 5：实现 handle_create（POST /api/issues）**

解析请求体 JSON，调用 `issue_create()`，返回新创建的 Issue 数据。

- [ ] **步骤 6：实现 handle_get（GET /api/issues/:id）**

调用 `issue_get_by_id()` + `checklist_list()`，返回 Issue 详情 + 待办清单。

- [ ] **步骤 7：实现 handle_update（PATCH /api/issues/:id）**

支持更新 status / title / description / labels，调用 `issue_update()`。

- [ ] **步骤 8：实现 handle_delete（DELETE /api/issues/:id）**

调用 `issue_delete()`。

- [ ] **步骤 9：实现 checklist 相关端点**

handle_checklist_add / handle_checklist_toggle / handle_checklist_delete。

- [ ] **步骤 10：实现 main.c — 服务器入口**

```c
#include "issue_handler.h"
#include "db/kv.h"
#include <microhttpd.h>
#include <stdio.h>
#include <signal.h>

static kv_t *g_db = NULL;
static struct MHD_Daemon *g_daemon = NULL;

static void signal_handler(int sig) {
    if (g_daemon) MHD_stop_daemon(g_daemon);
    if (g_db) kv_close(g_db);
    exit(0);
}

int main(int argc, char *argv[]) {
    int port = 8080;
    const char *db_path = "issue_tool.db";

    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) port = atoi(argv[++i]);
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) db_path = argv[++i];
    }

    // 打开数据库
    g_db = kv_open(db_path);
    if (!g_db) {
        fprintf(stderr, "无法打开数据库: %s\n", db_path);
        return 1;
    }

    // 初始化表
    if (issue_db_init(g_db) != 0) {
        fprintf(stderr, "数据库初始化失败\n");
        kv_close(g_db);
        return 1;
    }

    // 初始化处理器
    issue_handler_init(g_db);

    // 启动 HTTP 服务器
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    g_daemon = MHD_start_daemon(
        MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD,
        port, NULL, NULL,
        &issue_handler, NULL,
        MHD_OPTION_END);

    if (!g_daemon) {
        fprintf(stderr, "无法启动 HTTP 服务器 (端口: %d)\n", port);
        kv_close(g_db);
        return 1;
    }

    printf("GitHub Issue 待办工具已启动\n");
    printf("  服务器: http://localhost:%d\n", port);
    printf("  API:    http://localhost:%d/api/issues\n", port);
    printf("  按 Ctrl+C 停止\n");

    // 等待信号
    pause();

    return 0;
}
```

- [ ] **步骤 11：集成测试（curl）**

```bash
# 启动服务器
./build/engineering/apps/github-issue-todo/github-issue-todo -p 8080 -d test.db

# 创建 Issue
curl -X POST http://localhost:8080/api/issues \
  -H "Content-Type: application/json" \
  -d '{"title":"测试 Issue","description":"描述内容","labels":["bug"]}'

# 列表
curl "http://localhost:8080/api/issues?status=open"

# 详情
curl http://localhost:8080/api/issues/1

# 添加待办
curl -X POST http://localhost:8080/api/issues/1/checklist \
  -H "Content-Type: application/json" \
  -d '{"text":"完成设计文档"}'

# 切换待办状态
curl -X PATCH http://localhost:8080/api/issues/1/checklist/1

# 关闭 Issue
curl -X PATCH http://localhost:8080/api/issues/1 \
  -H "Content-Type: application/json" \
  -d '{"status":"closed"}'
```

- [ ] **步骤 12：提交**

```bash
git add engineering/apps/github-issue-todo/main.c engineering/apps/github-issue-todo/issue_handler.c
git add engineering/apps/github-issue-todo/issue_handler.h
git commit -m "feat(github-issue-todo): 实现 HTTP 服务器和 REST API"
```

---

### 任务 4：创建前端 Vue 3 SPA

**文件：**
- 创建：`engineering/apps/web/github-issue-todo/package.json`
- 创建：`engineering/apps/web/github-issue-todo/vite.config.js`
- 创建：`engineering/apps/web/github-issue-todo/index.html`
- 创建：`engineering/apps/web/github-issue-todo/src/main.js`
- 创建：`engineering/apps/web/github-issue-todo/src/App.vue`
- 创建：`engineering/apps/web/github-issue-todo/src/router.js`
- 创建：`engineering/apps/web/github-issue-todo/src/api.js`
- 创建：`engineering/apps/web/github-issue-todo/src/styles/main.css`
- 创建：`engineering/apps/web/github-issue-todo/src/components/IssueList.vue`
- 创建：`engineering/apps/web/github-issue-todo/src/components/IssueDetail.vue`
- 创建：`engineering/apps/web/github-issue-todo/src/components/IssueCreate.vue`
- 创建：`engineering/apps/web/github-issue-todo/src/components/FilterBar.vue`
- 创建：`engineering/apps/web/github-issue-todo/src/components/IssueCard.vue`
- 创建：`engineering/apps/web/github-issue-todo/src/components/Checklist.vue`

- [ ] **步骤 1：创建项目配置文件**

package.json:
```json
{
  "name": "github-issue-todo",
  "version": "1.0.0",
  "private": true,
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "vite build",
    "preview": "vite preview"
  },
  "dependencies": {
    "vue": "^3.4.0",
    "vue-router": "^4.3.0"
  },
  "devDependencies": {
    "@vitejs/plugin-vue": "^5.0.0",
    "vite": "^5.4.0"
  }
}
```

vite.config.js:
```js
import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig({
  plugins: [vue()],
  server: {
    port: 3000,
    proxy: {
      '/api': {
        target: 'http://localhost:8080',
        changeOrigin: true
      }
    }
  },
  build: {
    outDir: '../../github-issue-todo/web',
    emptyOutDir: true
  }
})
```

- [ ] **步骤 2：创建入口文件 index.html**

```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Issue 待办工具</title>
  <link rel="stylesheet" href="/src/styles/main.css">
</head>
<body>
  <div id="app"></div>
  <script type="module" src="/src/main.js"></script>
</body>
</html>
```

- [ ] **步骤 3：创建 main.js + App.vue + router.js**

main.js — 创建 Vue 应用实例，挂载路由
App.vue — 根组件，包含导航栏和 `<router-view>`
router.js — 路由配置：`/` → IssueList, `/create` → IssueCreate, `/issue/:id` → IssueDetail

- [ ] **步骤 4：创建 API 封装 api.js**

```js
const BASE_URL = '/api'

export async function fetchIssues(params = {}) {
  const query = new URLSearchParams()
  if (params.status) query.set('status', params.status)
  if (params.labels) query.set('labels', params.labels)
  if (params.search) query.set('search', params.search)
  if (params.page) query.set('page', params.page)
  if (params.per_page) query.set('per_page', params.per_page)

  const res = await fetch(`${BASE_URL}/issues?${query}`)
  return res.json()
}

export async function createIssue(data) {
  const res = await fetch(`${BASE_URL}/issues`, {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(data)
  })
  return res.json()
}

export async function getIssue(id) {
  const res = await fetch(`${BASE_URL}/issues/${id}`)
  return res.json()
}

export async function updateIssue(id, data) {
  const res = await fetch(`${BASE_URL}/issues/${id}`, {
    method: 'PATCH',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(data)
  })
  return res.json()
}

export async function deleteIssue(id) {
  const res = await fetch(`${BASE_URL}/issues/${id}`, {method: 'DELETE'})
  return res.json()
}

export async function addChecklistItem(issueId, text) {
  const res = await fetch(`${BASE_URL}/issues/${issueId}/checklist`, {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({text})
  })
  return res.json()
}

export async function toggleChecklistItem(issueId, itemId) {
  const res = await fetch(`${BASE_URL}/issues/${issueId}/checklist/${itemId}`, {
    method: 'PATCH'
  })
  return res.json()
}

export async function deleteChecklistItem(issueId, itemId) {
  const res = await fetch(`${BASE_URL}/issues/${issueId}/checklist/${itemId}`, {
    method: 'DELETE'
  })
  return res.json()
}
```

- [ ] **步骤 5：创建全局样式 main.css**

仿 GitHub Issues 风格的简洁 CSS，包含：
- 页面布局（导航栏、侧边筛选栏、内容区）
- 卡片样式（IssueCard）
- 表单样式（创建/编辑）
- 标签样式（标签圆角矩形）
- 待办清单样式（可勾选复选框）
- 响应式布局

- [ ] **步骤 6：创建 FilterBar.vue**

侧边筛选面板组件：
- 状态筛选：全部 / Open / Closed
- 标签筛选：标签列表（从后端获取或预设）
- 搜索框：关键词实时搜索（300ms 防抖）

- [ ] **步骤 7：创建 IssueList.vue**

列表页主组件：
- 顶部：新建按钮 + 统计（X Open / Y Closed）
- 中间：FilterBar
- 列表：IssueCard 列表
- 底部：分页

- [ ] **步骤 8：创建 IssueCard.vue**

单条 Issue 卡片组件：
- Issue 编号 + 标题
- 标签列表
- 状态徽标（Open=绿色, Closed=红色）
- 创建时间
- 点击跳转详情页

- [ ] **步骤 9：创建 IssueDetail.vue**

详情页主组件：
- 顶部：标题 + 状态标签 + 操作按钮（关闭/重新打开/删除）
- 主体：描述内容
- 待办清单区域：Checklist 组件
- 元信息：创建时间、更新时间

- [ ] **步骤 10：创建 Checklist.vue**

待办清单组件：
- 待办项列表，每项前有复选框
- 已完成项文字加删除线
- 底部输入框 + 添加按钮
- 每项右侧删除按钮

- [ ] **步骤 11：创建 IssueCreate.vue**

新建 Issue 页面：
- 标题输入框（必填）
- 描述文本域
- 标签多选（预设标签列表：bug / enhancement / question / documentation / urgent）
- 提交按钮
- 提交成功后跳转到详情页

- [ ] **步骤 12：安装依赖并测试**

```bash
cd engineering/apps/web/github-issue-todo
npm install
npm run dev
```
预期：Vite 开发服务器启动，浏览器打开后显示 Issue 列表页（此时后端也要运行）

- [ ] **步骤 13：提交**

```bash
git add engineering/apps/web/github-issue-todo/
git commit -m "feat(github-issue-todo): 添加 Vue 3 前端 SPA 页面"
```

---

### 任务 5：前端构建产物集成到后端

**文件：**
- 修改：`engineering/apps/github-issue-todo/main.c`（添加静态文件服务）
- 修改：`engineering/apps/web/github-issue-todo/vite.config.js`（确认构建输出目录）

- [ ] **步骤 1：配置 Vite 构建输出到后端 web 目录**

确认 `vite.config.js` 中的 `build.outDir` 指向 `../../github-issue-todo/web`

- [ ] **步骤 2：构建前端**

```bash
cd engineering/apps/web/github-issue-todo && npm run build
```
预期：产物输出到 `engineering/apps/github-issue-todo/web/`

- [ ] **步骤 3：后端添加静态文件路由**

在 main.c 中添加 libmicrohttpd 静态文件处理：
```c
// 处理静态文件请求
static enum MHD_Result static_file_handler(struct MHD_Connection *connection,
                                            const char *url) {
    // 拼接文件路径
    char filepath[512];
    if (strcmp(url, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "web/index.html");
    } else {
        snprintf(filepath, sizeof(filepath), "web%s", url);
    }

    // 打开文件
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return MHD_NO;  // 让 API 路由处理

    // 读取文件内容
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *data = malloc(fsize + 1);
    fread(data, 1, fsize, fp);
    data[fsize] = '\0';
    fclose(fp);

    // 确定 Content-Type
    const char *mime = "text/plain";
    if (strstr(url, ".html")) mime = "text/html";
    else if (strstr(url, ".js")) mime = "application/javascript";
    else if (strstr(url, ".css")) mime = "text/css";
    else if (strstr(url, ".json")) mime = "application/json";
    else if (strstr(url, ".png")) mime = "image/png";
    else if (strstr(url, ".svg")) mime = "image/svg+xml";

    struct MHD_Response *response = MHD_create_response_from_buffer(
        fsize, data, MHD_RESPMEM_MUST_FREE);
    MHD_add_response_header(response, "Content-Type", mime);
    enum MHD_Result ret = MHD_queue_response(connection, 200, response);
    MHD_destroy_response(response);
    return ret;
}
```

修改 `issue_handler` 中的路由逻辑：先尝试静态文件，再尝试 API。

- [ ] **步骤 4：构建后端并测试全链路**

```bash
cd c:/code/book && cmake --build build/engineering --target github-issue-todo
./build/engineering/apps/github-issue-todo/github-issue-todo -p 8080 -d test.db
```

浏览器打开 `http://localhost:8080/`，测试完整流程：创建 Issue → 列表 → 详情 → 添加待办 → 关闭。

- [ ] **步骤 5：提交**

```bash
git add engineering/apps/github-issue-todo/main.c engineering/apps/web/github-issue-todo/vite.config.js
git add engineering/apps/github-issue-todo/web/
git commit -m "feat(github-issue-todo): 集成前端构建产物到后端静态文件服务"
```

---

### 任务 6：测试与验收

**文件：**
- 创建：`engineering/test/db/test_issue_api.cpp`（API 集成测试）
- 修改：`engineering/test/db/CMakeLists.txt`（添加测试注册）

- [ ] **步骤 1：创建 API 集成测试**

```cpp
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>

extern "C" {
#include "db/kv.h"
#include "apps/github-issue-todo/issue_model.h"
#include "apps/github-issue-todo/issue_handler.h"
#include <microhttpd.h>
}

class IssueApiTest : public ::testing::Test {
protected:
    kv_t *db;
    struct MHD_Daemon *daemon;

    void SetUp() override {
        db = kv_open(":memory:");
        ASSERT_NE(db, nullptr);
        ASSERT_EQ(issue_db_init(db), 0);
        issue_handler_init(db);

        daemon = MHD_start_daemon(
            MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD,
            0, NULL, NULL, &issue_handler, NULL,
            MHD_OPTION_SOCK_ADDR, /* 使用随机端口 */,
            MHD_OPTION_END);
        ASSERT_NE(daemon, nullptr);
    }

    void TearDown() override {
        if (daemon) MHD_stop_daemon(daemon);
        if (db) kv_close(db);
    }
};

TEST_F(IssueApiTest, FullWorkflow) {
    // 1. 创建 Issue
    // 2. 列表验证
    // 3. 查看详情
    // 4. 添加待办
    // 5. 切换待办状态
    // 6. 关闭 Issue
    // 7. 验证列表状态变更
}
```

- [ ] **步骤 2：注册测试到 CMakeLists.txt**

在 `engineering/test/db/CMakeLists.txt` 中添加：
```cmake
add_project_test(test_issue_api DB_TEST_SOURCES)
target_link_libraries(test_issue_api PRIVATE
    ${ENGINEERING_SOURCE_DIR}/../third_part/libmicrohttpd/lib/libmicrohttpd.a
    ${ENGINEERING_SOURCE_DIR}/../third_part/cjson/libcjson.a
)
```

- [ ] **步骤 3：运行全部测试**

```bash
cd c:/code/book && ctest --test-dir build/engineering -R "test_issue" --output-on-failure
```
预期：所有测试 PASS

- [ ] **步骤 4：更新 tasks.md**

将 openspec 变更的 tasks.md 中的任务标记为完成。

- [ ] **步骤 5：最终提交**

```bash
git add engineering/test/db/test_issue_api.cpp engineering/test/db/CMakeLists.txt
git commit -m "test(github-issue-todo): 添加 API 集成测试"
```