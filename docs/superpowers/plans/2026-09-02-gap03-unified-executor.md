# Gap#3 统一执行引擎实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现统一执行引擎框架，将 Optimizer 输出的 plan_node_t 转换为 ExecNode 执行树，通过 Volcano Pull-based 模型驱动向量化算子。

**Architecture:** Executor 作为调度器，通过 Plan → Exec 转换层接收优化器计划，每个 ExecNode 持有函数指针表（open/next/close/reset），内部委托给 vecx_* 向量化算子实现。

**Tech Stack:** C 语言，CMake 构建，GTest 单元测试

## Global Constraints

- 向量化算子接口参考 `engineering/include/db/vectorized/vectorized.h`
- plan_node_type_t 枚举定义在 `engineering/include/db/optimizer/optimizer.h`
- VectorBlock 定义在 `engineering/include/db/core/vector_types.h`
- 遵循现有代码风格（extern "C"、命名下划线分隔）
- 所有新文件加入对应 CMakeLists.txt

---

### Task 1: ExecNode 基结构与状态定义

**Files:**
- Create: `engineering/include/db/executor/exec_node.h`
- Create: `engineering/include/db/executor/exec_states.h`
- Create: `engineering/include/db/executor/executor_framework.h`
- Modify: `engineering/src/db/executor/CMakeLists.txt`（添加 framework 子目录）

**Interfaces:**
- Consumes: `plan_node_type_t`（optimizer.h），`VectorBlock`（vector_types.h），`vecx_pred_t`（vectorized.h）
- Produces: `ExecNode`、`ExecNodeState` 基结构、生命周期函数声明

- [ ] **Step 1: 创建 exec_node.h — ExecNode 基结构**

```c
// engineering/include/db/executor/exec_node.h
#ifndef DB_EXECUTOR_EXEC_NODE_H
#define DB_EXECUTOR_EXEC_NODE_H

#include <stdint.h>
#include <stdbool.h>
#include "db/optimizer/optimizer.h"
#include "db/core/vector_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 执行节点基结构
 *
 * 每个算子实现需提供 open/next/close/reset 四个函数指针。
 * next() 返回 VectorBlock 指针，NULL 表示迭代结束。
 */
typedef struct ExecNode {
    plan_node_type_t node_type;     /**< 节点类型（PLAN_SCAN_SEQ 等） */
    struct ExecNode *left;          /**< 左子节点 */
    struct ExecNode *right;         /**< 右子节点（Join 用） */
    void *state;                    /**< 算子私有状态 */

    /**
     * @brief 初始化算子（相当于 PostgreSQL 的 ExecInit）
     * @return 0 成功，-1 失败
     */
    int (*open)(struct ExecNode *node);

    /**
     * @brief 获取下一批数据
     * @return VectorBlock 指针，NULL 表示迭代结束
     */
    struct VectorBlock *(*next)(struct ExecNode *node);

    /**
     * @brief 重置算子状态（用于迭代重启）
     */
    void (*reset)(struct ExecNode *node);

    /**
     * @brief 关闭算子，释放资源（相当于 PostgreSQL 的 ExecEnd）
     */
    void (*close)(struct ExecNode *node);
} ExecNode;

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_NODE_H */
```

- [ ] **Step 2: 创建 exec_states.h — 各算子状态结构**

```c
// engineering/include/db/executor/exec_states.h
#ifndef DB_EXECUTOR_EXEC_STATES_H
#define DB_EXECUTOR_EXEC_STATES_H

#include "exec_node.h"
#include "db/vectorized/vectorized.h"

/**
 * @brief SeqScan 状态
 */
typedef struct {
    int table_id;
    int ncols;
    int *col_types;
    void **col_data;
    int *col_elem_size;
    int64_t total_rows;
    int batch_size;
    vecx_source_t *source;
    VectorBlock *cur_block;
    int exhausted;
} SeqScanState;

/**
 * @brief Filter 状态
 */
typedef struct {
    vecx_pred_t pred;
    VectorBlock *cur_block;
    int exhausted;
} FilterState;

/**
 * @brief Project 状态
 */
typedef struct {
    vecx_expr_t *expr;
    VectorBlock *cur_block;
    int exhausted;
} ProjectState;

/**
 * @brief HashJoin 状态
 */
typedef struct {
    vecx_hashjoin_t *hj;
    VectorBlock *cur_block;
    int exhausted;
    int build_done;
} HashJoinState;

/**
 * @brief HashAgg 状态
 */
typedef struct {
    vecx_hashagg_t *agg;
    VectorBlock *cur_block;
    int emitted;
    int exhausted;
} HashAggState;

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_STATES_H */
```

- [ ] **Step 3: 创建 executor_framework.h — Executor 公共接口**

```c
// engineering/include/db/executor/executor_framework.h
#ifndef DB_EXECUTOR_FRAMEWORK_H
#define DB_EXECUTOR_FRAMEWORK_H

#include "exec_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建执行树（Plan → ExecNode 转换）
 * @param plan 优化器输出的 plan_node_t 树
 * @return ExecNode 树，失败返回 NULL
 */
ExecNode *exec_create(const plan_node_t *plan);

/**
 * @brief 初始化执行树（自底向上调用 open）
 * @param root 执行树根节点
 * @return 0 成功，-1 失败
 */
int exec_open(ExecNode *root);

/**
 * @brief 获取下一批数据（从根节点驱动）
 * @param node 执行节点
 * @return VectorBlock 指针，NULL 表示迭代结束
 */
VectorBlock *exec_next(ExecNode *node);

/**
 * @brief 重置执行树（用于迭代重启）
 * @param root 执行树根节点
 */
void exec_reset(ExecNode *root);

/**
 * @brief 关闭执行树，释放资源（自顶向下调用 close）
 * @param root 执行树根节点
 */
void exec_close(ExecNode *root);

/**
 * @brief 销毁执行树，释放 ExecNode 树
 * @param root 执行树根节点
 */
void exec_destroy(ExecNode *root);

/**
 * @brief 一键执行接口
 * @param plan 优化器输出的 plan_node_t 树
 * @param has_result 输出：是否有结果
 * @return 结果 VectorBlock，has_result=0 时返回 NULL
 */
VectorBlock *exec_exec(const plan_node_t *plan, int *has_result);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_FRAMEWORK_H */
```

- [ ] **Step 4: 更新 CMakeLists.txt**

```cmake
# engineering/src/db/executor/CMakeLists.txt
add_subdirectory(framework)
# operators 子目录在 Task 3 创建后添加
```

- [ ] **Step 5: 提交**

```bash
git add engineering/include/db/executor/exec_node.h \
        engineering/include/db/executor/exec_states.h \
        engineering/include/db/executor/executor_framework.h \
        engineering/src/db/executor/CMakeLists.txt
git commit -m "feat(executor): Add ExecNode base structure and framework headers"
```

---

### Task 2: Plan → ExecNode 转换层

**Files:**
- Create: `engineering/src/db/executor/framework/plan_to_exec.c`
- Create: `engineering/src/db/executor/framework/CMakeLists.txt`

**Interfaces:**
- Consumes: `plan_node_t`（optimizer.h），Task 1 的 ExecNode 结构
- Produces: `exec_create()` 函数

- [ ] **Step 1: 创建 plan_to_exec.c — Plan → ExecNode 转换**

```c
// engineering/src/db/executor/framework/plan_to_exec.c
#include "db/executor/executor_framework.h"
#include "db/executor/exec_node.h"
#include "db/executor/exec_states.h"
#include <stdlib.h>
#include <string.h>

static ExecNode *plan_to_exec_impl(const plan_node_t *plan);

/**
 * @brief 递归转换单个 plan 节点
 */
static ExecNode *convert_scan_node(const plan_node_t *plan) {
    SeqScanState *state = (SeqScanState *)calloc(1, sizeof(SeqScanState));
    if (!state) return NULL;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        free(state);
        return NULL;
    }

    node->node_type = plan->type;
    node->state = state;
    // 函数指针在算子实现文件中设置
    return node;
}

static ExecNode *convert_filter_node(const plan_node_t *plan) {
    FilterState *state = (FilterState *)calloc(1, sizeof(FilterState));
    if (!state) return NULL;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        free(state);
        return NULL;
    }

    node->node_type = plan->type;
    node->state = state;
    // 子节点
    if (plan->left) {
        node->left = plan_to_exec_impl(plan->left);
    }
    return node;
}

static ExecNode *plan_to_exec_impl(const plan_node_t *plan) {
    if (!plan) return NULL;

    switch (plan->type) {
        case PLAN_SCAN_SEQ:
        case PLAN_SCAN_INDEX:
        case PLAN_SCAN_VECTOR:
            return convert_scan_node(plan);
        case PLAN_FILTER:
            return convert_filter_node(plan);
        case PLAN_PROJECT:
        case PLAN_JOIN_HASH:
        case PLAN_AGGREGATE:
        case PLAN_SORT:
            // TODO: Task 3 实现
            return NULL;
        default:
            return NULL;
    }
}

ExecNode *exec_create(const plan_node_t *plan) {
    return plan_to_exec_impl(plan);
}
```

- [ ] **Step 2: 创建 framework/CMakeLists.txt**

```cmake
# engineering/src/db/executor/framework/CMakeLists.txt
add_library(executor_framework STATIC
    plan_to_exec.c
    executor_framework.c  # Task 3 创建
)
target_link_libraries(executor_framework
    PUBLIC db_core db_vectorized
)
```

- [ ] **Step 3: 提交**

```bash
git add engineering/src/db/executor/framework/plan_to_exec.c \
        engineering/src/db/executor/framework/CMakeLists.txt
git commit -m "feat(executor): Add Plan → ExecNode conversion layer"
```

---

### Task 3: Executor 调度器核心

**Files:**
- Create: `engineering/src/db/executor/framework/executor_framework.c`
- Modify: `engineering/src/db/executor/framework/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 的 executor_framework.h
- Produces: `exec_open/exec_next/exec_reset/exec_close/exec_destroy/exec_exec` 实现

- [ ] **Step 1: 创建 executor_framework.c**

```c
// engineering/src/db/executor/framework/executor_framework.c
#include "db/executor/executor_framework.h"
#include "db/executor/exec_node.h"
#include <stdlib.h>

/**
 * @brief 递归初始化（自底向上）
 */
static int exec_open_impl(ExecNode *node) {
    if (!node) return 0;

    // 先初始化子节点
    if (node->left && exec_open_impl(node->left) != 0) return -1;
    if (node->right && exec_open_impl(node->right) != 0) return -1;

    // 再初始化当前节点
    if (node->open && node->open(node) != 0) return -1;

    return 0;
}

int exec_open(ExecNode *root) {
    return exec_open_impl(root);
}

/**
 * @brief 获取下一批数据（委托给节点的 next 函数）
 */
VectorBlock *exec_next(ExecNode *node) {
    if (!node || !node->next) return NULL;
    return node->next(node);
}

/**
 * @brief 递归重置
 */
static void exec_reset_impl(ExecNode *node) {
    if (!node) return;
    if (node->reset) node->reset(node);
    exec_reset_impl(node->left);
    exec_reset_impl(node->right);
}

void exec_reset(ExecNode *root) {
    exec_reset_impl(root);
}

/**
 * @brief 递归关闭（自顶向下）
 */
static void exec_close_impl(ExecNode *node) {
    if (!node) return;

    // 先关闭当前节点
    if (node->close) node->close(node);

    // 再关闭子节点
    exec_close_impl(node->left);
    exec_close_impl(node->right);
}

void exec_close(ExecNode *root) {
    exec_close_impl(root);
}

/**
 * @brief 递归销毁
 */
static void exec_destroy_impl(ExecNode *node) {
    if (!node) return;

    exec_destroy_impl(node->left);
    exec_destroy_impl(node->right);

    if (node->state) free(node->state);
    free(node);
}

void exec_destroy(ExecNode *root) {
    exec_destroy_impl(root);
}

/**
 * @brief 一键执行
 */
VectorBlock *exec_exec(const plan_node_t *plan, int *has_result) {
    ExecNode *root = exec_create(plan);
    if (!root) {
        if (has_result) *has_result = 0;
        return NULL;
    }

    if (exec_open(root) != 0) {
        exec_destroy(root);
        if (has_result) *has_result = 0;
        return NULL;
    }

    VectorBlock *result = exec_next(root);
    if (has_result) *has_result = (result != NULL) ? 1 : 0;

    exec_close(root);
    exec_destroy(root);

    return result;
}
```

- [ ] **Step 2: 更新 CMakeLists.txt**

```cmake
# engineering/src/db/executor/framework/CMakeLists.txt
add_library(executor_framework STATIC
    plan_to_exec.c
    executor_framework.c
)
target_link_libraries(executor_framework
    PUBLIC db_core db_vectorized
)
```

- [ ] **Step 3: 提交**

```bash
git add engineering/src/db/executor/framework/executor_framework.c \
        engineering/src/db/executor/framework/CMakeLists.txt
git commit -m "feat(executor): Add executor framework core - open/next/close/destroy"
```

---

### Task 4: SeqScan 算子实现

**Files:**
- Create: `engineering/src/db/executor/operators/seqscan_exec.c`
- Create: `engineering/src/db/executor/operators/CMakeLists.txt`
- Modify: `engineering/src/db/executor/CMakeLists.txt`

**Interfaces:**
- Consumes: `exec_node.h`、`exec_states.h`、`vecx_source_from_columns`
- Produces: SeqScan 的 open/next/close/reset 函数实现

- [ ] **Step 1: 创建 seqscan_exec.c**

```c
// engineering/src/db/executor/operators/seqscan_exec.c
#include "db/executor/exec_node.h"
#include "db/executor/exec_states.h"
#include "db/vectorized/vectorized.h"
#include <stdlib.h>

static int seqscan_open(ExecNode *node) {
    SeqScanState *state = (SeqScanState *)node->state;
    if (!state) return -1;

    state->source = vecx_source_from_columns(
        state->ncols,
        state->col_types,
        (const void **)state->col_data,
        state->col_elem_size,
        state->total_rows,
        state->batch_size
    );

    if (!state->source) return -1;
    state->exhausted = 0;
    state->cur_block = NULL;
    return 0;
}

static VectorBlock *seqscan_next(ExecNode *node) {
    SeqScanState *state = (SeqScanState *)node->state;
    if (!state || state->exhausted) return NULL;

    VectorBlock *block = vecx_source_next(state->source);
    if (!block) {
        state->exhausted = 1;
        return NULL;
    }
    return block;
}

static void seqscan_reset(ExecNode *node) {
    SeqScanState *state = (SeqScanState *)node->state;
    if (!state) return;
    state->exhausted = 0;
}

static void seqscan_close(ExecNode *node) {
    SeqScanState *state = (SeqScanState *)node->state;
    if (!state) return;

    if (state->source) {
        vecx_source_destroy(state->source);
        state->source = NULL;
    }
}

/**
 * @brief 创建 SeqScan ExecNode
 */
ExecNode *exec_create_seqscan(
    int table_id,
    int ncols,
    int *col_types,
    void **col_data,
    int *col_elem_size,
    int64_t total_rows,
    int batch_size
) {
    SeqScanState *state = (SeqScanState *)calloc(1, sizeof(SeqScanState));
    if (!state) return NULL;

    state->table_id = table_id;
    state->ncols = ncols;
    state->col_types = col_types;
    state->col_data = col_data;
    state->col_elem_size = col_elem_size;
    state->total_rows = total_rows;
    state->batch_size = batch_size;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        free(state);
        return NULL;
    }

    node->node_type = PLAN_SCAN_SEQ;
    node->state = state;
    node->open = seqscan_open;
    node->next = seqscan_next;
    node->reset = seqscan_reset;
    node->close = seqscan_close;

    return node;
}
```

- [ ] **Step 2: 创建 operators/CMakeLists.txt**

```cmake
# engineering/src/db/executor/operators/CMakeLists.txt
add_library(executor_operators STATIC
    seqscan_exec.c
    filter_exec.c    # Task 5 创建
    project_exec.c   # Task 5 创建
    hashjoin_exec.c  # Task 5 创建
    hashagg_exec.c   # Task 5 创建
)
target_link_libraries(executor_operators
    PUBLIC executor_framework db_vectorized
)
```

- [ ] **Step 3: 更新主 CMakeLists.txt**

```cmake
# engineering/src/db/executor/CMakeLists.txt
add_subdirectory(framework)
add_subdirectory(operators)
```

- [ ] **Step 4: 更新 plan_to_exec.c 支持 SeqScan**

在 plan_to_exec.c 的 `convert_scan_node` 中添加：
```c
node->open = seqscan_open;
node->next = seqscan_next;
node->reset = seqscan_reset;
node->close = seqscan_close;
```

- [ ] **Step 5: 提交**

```bash
git add engineering/src/db/executor/operators/seqscan_exec.c \
        engineering/src/db/executor/operators/CMakeLists.txt \
        engineering/src/db/executor/CMakeLists.txt \
        engineering/src/db/executor/framework/plan_to_exec.c
git commit -m "feat(executor): Add SeqScan operator implementation"
```

---

### Task 5: Filter/Project/HashJoin/HashAgg 算子实现

**Files:**
- Create: `engineering/src/db/executor/operators/filter_exec.c`
- Create: `engineering/src/db/executor/operators/project_exec.c`
- Create: `engineering/src/db/executor/operators/hashjoin_exec.c`
- Create: `engineering/src/db/executor/operators/hashagg_exec.c`
- Modify: `engineering/src/db/executor/operators/CMakeLists.txt`

**Interfaces:**
- Consumes: `exec_node.h`、`exec_states.h`、各 `vecx_*` 函数
- Produces: Filter/Project/HashJoin/HashAgg 的 open/next/close/reset 实现

- [ ] **Step 1: 创建 filter_exec.c**

```c
// engineering/src/db/executor/operators/filter_exec.c
#include "db/executor/exec_node.h"
#include "db/executor/exec_states.h"
#include "db/vectorized/vectorized.h"
#include <stdlib.h>

static int filter_open(ExecNode *node) {
    FilterState *state = (FilterState *)node->state;
    if (!state) return -1;

    // 子节点已在 exec_open 时初始化
    state->exhausted = 0;
    state->cur_block = NULL;
    return 0;
}

static VectorBlock *filter_next(ExecNode *node) {
    FilterState *state = (FilterState *)node->state;
    if (!state || state->exhausted) return NULL;

    // 从子节点获取数据并过滤
    VectorBlock *input = exec_next(node->left);
    if (!input) {
        state->exhausted = 1;
        return NULL;
    }

    VectorBlock *output = NULL;
    int n = vecx_filter_block(input, state->pred.col, state->pred.op,
                              &state->pred.i64, &output);
    (void)n;  // 未使用

    return output;
}

static void filter_reset(ExecNode *node) {
    FilterState *state = (FilterState *)node->state;
    if (!state) return;

    state->exhausted = 0;
    if (node->left && node->left->reset) {
        node->left->reset(node->left);
    }
}

static void filter_close(ExecNode *node) {
    FilterState *state = (FilterState *)node->state;
    if (!state) return;
    state->cur_block = NULL;
}

ExecNode *exec_create_filter(const vecx_pred_t *pred) {
    FilterState *state = (FilterState *)calloc(1, sizeof(FilterState));
    if (!state) return NULL;

    state->pred = *pred;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        free(state);
        return NULL;
    }

    node->node_type = PLAN_FILTER;
    node->state = state;
    node->open = filter_open;
    node->next = filter_next;
    node->reset = filter_reset;
    node->close = filter_close;

    return node;
}
```

- [ ] **Step 2: 创建 project_exec.c（类似结构）**

- [ ] **Step 3: 创建 hashjoin_exec.c（两子节点：left=build, right=probe）**

- [ ] **Step 4: 创建 hashagg_exec.c**

- [ ] **Step 5: 更新 CMakeLists.txt**

- [ ] **Step 6: 更新 plan_to_exec.c 支持所有算子**

- [ ] **Step 7: 提交**

---

### Task 6: 单元测试

**Files:**
- Create: `engineering/test/db/executor/framework_test.cpp`
- Create: `engineering/test/db/executor/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1-5 实现
- Produces: 各算子单元测试

- [ ] **Step 1: 创建 framework_test.cpp**

```cpp
// engineering/test/db/executor/framework_test.cpp
#include <gtest/gtest.h>
#include "db/executor/executor_framework.h"
#include "db/executor/exec_states.h"

class ExecutorFrameworkTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 测试 SeqScan 基本流程
TEST_F(ExecutorFrameworkTest, SeqScanBasic) {
    // 构造测试数据
    int ncols = 2;
    int col_types[] = {COLUMN_INT32, COLUMN_INT32};
    int32_t col0[] = {1, 2, 3, 4, 5};
    int32_t col1[] = {10, 20, 30, 40, 50};
    void *col_data[] = {col0, col1};
    int col_elem_size[] = {sizeof(int32_t), sizeof(int32_t)};

    // 构造 plan_node
    plan_node_t plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = PLAN_SCAN_SEQ;

    // 创建并执行
    ExecNode *root = exec_create(&plan);
    ASSERT_NE(root, nullptr);

    EXPECT_EQ(exec_open(root), 0);

    int count = 0;
    VectorBlock *block;
    while ((block = exec_next(root)) != NULL) {
        count++;
        vector_block_destroy(block);
    }

    EXPECT_GT(count, 0);

    exec_close(root);
    exec_destroy(root);
}

// 测试 Filter 基本流程
TEST_F(ExecutorFrameworkTest, FilterBasic) {
    // 构造带子节点的 plan
    plan_node_t scan_plan;
    memset(&scan_plan, 0, sizeof(scan_plan));
    scan_plan.type = PLAN_SCAN_SEQ;

    plan_node_t filter_plan;
    memset(&filter_plan, 0, sizeof(filter_plan));
    filter_plan.type = PLAN_FILTER;
    filter_plan.left = &scan_plan;

    ExecNode *root = exec_create(&filter_plan);
    ASSERT_NE(root, nullptr);

    EXPECT_EQ(exec_open(root), 0);

    int count = 0;
    VectorBlock *block;
    while ((block = exec_next(root)) != NULL) {
        count++;
        vector_block_destroy(block);
    }

    exec_close(root);
    exec_destroy(root);
}

// 测试 exec_exec 一键执行
TEST_F(ExecutorFrameworkTest, ExecExecBasic) {
    plan_node_t plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = PLAN_SCAN_SEQ;

    int has_result = 0;
    VectorBlock *result = exec_exec(&plan, &has_result);

    // SeqScan 返回 NULL（无数据源）
    EXPECT_EQ(has_result, 0);
    EXPECT_EQ(result, nullptr);
}

// 测试 NULL plan
TEST_F(ExecutorFrameworkTest, NullPlan) {
    ExecNode *root = exec_create(NULL);
    EXPECT_EQ(root, nullptr);
}

// 测试 open 失败
TEST_F(ExecutorFrameworkTest, OpenFailure) {
    // 创建无效 plan
    plan_node_t plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = PLAN_SCAN_SEQ;  // 无数据源会失败

    ExecNode *root = exec_create(&plan);
    if (root) {
        // open 可能失败
        exec_destroy(root);
    }
}
```

- [ ] **Step 2: 创建 test CMakeLists.txt**

```cmake
# engineering/test/db/executor/CMakeLists.txt
add_executable(executor_framework_test
    framework_test.cpp
)
target_link_libraries(executor_framework_test
    PRIVATE db_executor db_vectorized gtest
)
target_include_directories(executor_framework_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/engineering/include
)
gtest_discover_tests(executor_framework_test)
add_test(NAME executor_framework_test COMMAND executor_framework_test)
```

- [ ] **Step 3: 更新 test db CMakeLists.txt**

```cmake
# engineering/test/db/CMakeLists.txt
add_subdirectory(executor)
```

- [ ] **Step 4: 编译测试验证**

```bash
cd build/engineering
cmake --build . --target executor_framework_test -j 4
ctest -R executor_framework_test --output-on-failure
```

- [ ] **Step 5: 提交**

---

### Task 7: 集成测试

**Files:**
- Create: `engineering/test/db/executor/integration_test.cpp`
- Modify: `engineering/test/db/executor/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1-6 全部实现
- Produces: 端到端集成测试

- [ ] **Step 1: 创建 integration_test.cpp**

```cpp
// engineering/test/db/executor/integration_test.cpp
#include <gtest/gtest.h>
#include "db/executor/executor_framework.h"
#include "db/optimizer/optimizer.h"
#include "db/parser/sql/sql.h"

// 测试端到端：解析 → 优化 → 执行
TEST(IntegrationTest, SelectFilterAggregate) {
    // 1. 解析 SQL
    const char *sql = "SELECT * FROM test WHERE id > 10";
    void *ast = sql_parse(sql);
    ASSERT_NE(ast, nullptr);

    // 2. 优化
    plan_node_t *plan = optimizer_optimize(ast);
    ASSERT_NE(plan, nullptr);

    // 3. 执行
    int has_result = 0;
    VectorBlock *result = exec_exec(plan, &has_result);

    // 验证结果
    if (has_result && result) {
        vector_block_destroy(result);
    }

    // 清理
    plan_node_destroy(plan);
    sql_parse_free(ast);
}

// 测试 HashJoin 端到端
TEST(IntegrationTest, HashJoin) {
    const char *sql = "SELECT * FROM t1 JOIN t2 ON t1.id = t2.id";
    void *ast = sql_parse(sql);
    ASSERT_NE(ast, nullptr);

    plan_node_t *plan = optimizer_optimize(ast);
    ASSERT_NE(plan, nullptr);

    int has_result = 0;
    VectorBlock *result = exec_exec(plan, &has_result);

    if (has_result && result) {
        vector_block_destroy(result);
    }

    plan_node_destroy(plan);
    sql_parse_free(ast);
}
```

- [ ] **Step 2: 更新 CMakeLists.txt**

- [ ] **Step 3: 提交**

---

## 任务依赖关系

```
Task 1: ExecNode 基结构  ← 基础
Task 2: Plan → Exec 转换  ← 依赖 Task 1
Task 3: Executor 调度器   ← 依赖 Task 1, 2
Task 4: SeqScan          ← 依赖 Task 1, 2, 3
Task 5: 其他算子         ← 依赖 Task 1, 2, 3
Task 6: 单元测试         ← 依赖 Task 1-5
Task 7: 集成测试         ← 依赖 Task 1-6
```

## 成功标准

- [ ] Task 1: ExecNode 基结构完整
- [ ] Task 2: Plan → ExecNode 转换工作
- [ ] Task 3: Executor 生命周期正确
- [ ] Task 4: SeqScan 算子工作
- [ ] Task 5: Filter/Project/HashJoin/HashAgg 算子工作
- [ ] Task 6: 单元测试全部通过
- [ ] Task 7: 集成测试通过

---

**Plan complete and saved to `docs/superpowers/plans/2026-09-02-gap03-unified-executor.md`.**

**Two execution options:**

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
