# P2-2 文档聚合管道 设计

> 日期：2026-08-28
> 状态：与提案一致
> 目标：实现类 MongoDB 风格的文档聚合管道

## 一、架构

文档聚合管道由多个 Stage 串联组成，每个 Stage 接收上游 Document 流，
处理后输出到下游。最终 Stage 的输出即查询结果。

```
Document 流 → [$match] → [$group] → [$sort] → [$limit/$skip] → 结果
```

## 二、Stage 接口

```c
typedef struct doc_stage_s doc_stage_t;

typedef struct doc_stage_ops_s {
    const char *name;                          /* "$match"/"$group"/... */
    int (*execute)(doc_stage_t *self,
                   doc_document_t **docs, size_t n_docs,
                   doc_document_t ***out_docs, size_t *out_n);
    void (*destroy)(doc_stage_t *self);
} doc_stage_ops_t;
```

## 三、Stage 类型

- **$match**：按 JSONPath 表达式过滤
- **$group**：按字段分组，累加器函数（$sum, $avg, $count, $min, $max）
- **$sort**：按字段排序（升/降序）
- **$limit** / **$skip**：分页

## 四、表达式计算

JSONPath 字段访问 + 算术运算 + 字符串操作。
简化版 JSONPath：仅支持点号分隔字段访问，无通配符/递归。

## 五、管道执行

```c
doc_pipeline_t *pipe = doc_pipeline_create();
doc_pipeline_add_stage(pipe, match_stage);
doc_pipeline_add_stage(pipe, group_stage);
doc_pipeline_add_stage(pipe, sort_stage);
doc_pipeline_execute(pipe, input_docs, n, &out_docs, &out_n);
```

## 六、已知限制（待优化）

1. 管道阶段内存管理存在潜在泄漏
2. JSON 表达式解析为简化实现
3. $match 不支持复杂组合条件
4. $group 累加器实现需完善

## 七、文件清单

创建：
- `engineering/include/db/storage/doc/doc_pipeline.h`
- `engineering/src/db/storage/doc/doc_pipeline.c`

测试：
- `engineering/test/db/storage/doc/doc_pipeline_test.cpp`