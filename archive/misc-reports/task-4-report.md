# Task 4 报告：EXPLAIN 执行计划分析工具

## STATUS: DONE

## COMMITS

```
d87ea3e8 feat(explain): 实现 EXPLAIN 执行计划分析工具
```

## TESTS

```
[==========] Running 22 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 22 tests from ExplainTest
[ RUN      ] ExplainTest.DefaultOptions
[       OK ] ExplainTest.DefaultOptions (0 ms)
[ RUN      ] ExplainTest.CreateDestroy
[       OK ] ExplainTest.CreateDestroy (0 ms)
[ RUN      ] ExplainTest.CreateWithNullOptions
[       OK ] ExplainTest.CreateWithNullOptions (0 ms)
[ RUN      ] ExplainTest.CreateNode
[       OK ] ExplainTest.CreateNode (0 ms)
[ RUN      ] ExplainTest.CreateNodeAllTypes
[       OK ] ExplainTest.CreateNodeAllTypes (0 ms)
[ RUN      ] ExplainTest.FreePlanNull
[       OK ] ExplainTest.FreePlanNull (0 ms)
[ RUN      ] ExplainTest.AddChild
[       OK ] ExplainTest.AddChild (0 ms)
[ RUN      ] ExplainTest.AddMultipleChildren
[       OK ] ExplainTest.AddMultipleChildren (0 ms)
[ RUN      ] ExplainTest.AddChildNullParent
[       OK ] ExplainTest.AddChildNullParent (0 ms)
[ RUN      ] ExplainTest.AddChildNullChild
[       OK ] ExplainTest.AddChildNullChild (0 ms)
[ RUN      ] ExplainTest.OutputTextSimple
[       OK ] ExplainTest.OutputTextSimple (0 ms)
[ RUN      ] ExplainTest.OutputTextWithFilter
[       OK ] ExplainTest.OutputTextWithFilter (0 ms)
[ RUN      ] ExplainTest.OutputTextWithChildren
[       OK ] ExplainTest.OutputTextWithChildren (0 ms)
[ RUN      ] ExplainTest.OutputTextNullArgs
[       OK ] ExplainTest.OutputTextNullArgs (0 ms)
[ RUN      ] ExplainTest.OutputJsonSimple
[       OK ] ExplainTest.OutputJsonSimple (0 ms)
[ RUN      ] ExplainTest.OutputJsonWithChildren
[       OK ] ExplainTest.OutputJsonWithChildren (0 ms)
[ RUN      ] ExplainTest.OutputJsonWithIndex
[       OK ] ExplainTest.OutputJsonWithIndex (0 ms)
[ RUN      ] ExplainTest.OutputTextWithAnalyze
[       OK ] ExplainTest.OutputTextWithAnalyze (0 ms)
[ RUN      ] ExplainTest.OutputJsonWithAnalyze
[       OK ] ExplainTest.OutputJsonWithAnalyze (0 ms)
[ RUN      ] ExplainTest.OutputTextWithBuffers
[       OK ] ExplainTest.OutputTextWithBuffers (0 ms)
[ RUN      ] ExplainTest.OutputJsonWithBuffers
[       OK ] ExplainTest.OutputJsonWithBuffers (0 ms)
[ RUN      ] ExplainTest.DeepNestedPlan
[       OK ] ExplainTest.DeepNestedPlan (0 ms)
[----------] 22 tests from ExplainTest (5 ms total)

[----------] Global test environment tear-down.
[==========] 22 tests from 1 test suite ran. (5 ms total)
[  PASSED  ] 22 tests.
```

## 实现概述

### 文件结构

```
engineering/include/db/tools/explain.h        # 公共 API 头文件
engineering/src/db/tools/explain/
├── CMakeLists.txt                            # 构建配置
├── explain.c                                 # 核心实现
├── explain_text.c                            # TEXT 格式输出
└── explain_json.c                            # JSON 格式输出
engineering/test/db/tools/test_explain.cpp    # 测试套件
```

### API 设计

```c
/* 默认选项 */
ExplainOptions explain_default_options(void);

/* 上下文管理 */
ExplainContext *explain_create(ExplainOptions *options);
void explain_destroy(ExplainContext *ctx);

/* 节点管理 */
ExplainPlanNode *explain_create_node(PlanNodeType type);
int explain_add_child(ExplainPlanNode *parent, ExplainPlanNode *child);
void explain_free_plan(ExplainPlanNode *node);

/* 输出生成 */
int explain_output(ExplainContext *ctx, ExplainPlanNode *plan, FILE *fp);
```

### 支持的计划节点类型

- `PLAN_NODE_SEQ_SCAN` - 顺序扫描
- `PLAN_NODE_INDEX_SCAN` - 索引扫描
- `PLAN_NODE_BITMAP_SCAN` - 位图扫描
- `PLAN_NODE_NESTED_LOOP` - 嵌套循环连接
- `PLAN_NODE_HASH_JOIN` - 哈希连接
- `PLAN_NODE_MERGE_JOIN` - 归并连接
- `PLAN_NODE_AGGREGATE` - 聚合
- `PLAN_NODE_SORT` - 排序
- `PLAN_NODE_LIMIT` - LIMIT
- `PLAN_NODE_GATHER` - Gather（并行）

### 输出格式

#### TEXT 格式（参考 PostgreSQL）

```
-> Seq Scan on users  (cost=0.00..100.00 rows=1000 width=64)
   Filter: (age > 18)
```

#### JSON 格式

```json
{
  "Plan": {
    "Node Type": "Seq Scan",
    "Relation Name": "users",
    "Startup Cost": 0.00,
    "Total Cost": 100.00,
    "Plan Rows": 1000,
    "Plan Width": 64,
    "Filter": "(age > 18)"
  }
}
```

### 支持的选项

- `analyze` - 实际执行并显示运行时信息
- `costs` - 显示成本估算
- `buffers` - 显示缓冲区使用情况
- `timing` - 显示时间信息
- `verbose` - 显示详细信息
