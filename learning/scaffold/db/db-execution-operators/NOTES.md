# NOTES.md - 执行算子工程对照

## 工程源码位置

`engineering/src/db/sql/sql_executor.c` (753 行)

## 火山模型 (Volcano Iterator Model)

```c
// 每个算子实现三接口
typedef struct PlanState_s {
    TupleTableSlot *(*exec_proc)(struct PlanState_s *);
    struct PlanState_s *left;
    struct PlanState_s *right;
    ExprContext *expr_context;
} PlanState;

// 算子函数
TupleTableSlot *exec_seq_scan(SeqScanState *node);
TupleTableSlot *exec_index_scan(IndexScanState *node);
TupleTableSlot *exec_hashjoin(HashJoinState *node);
TupleTableSlot *exec_nestloop(NestLoopState *node);
TupleTableSlot *exec_hash_agg(AggState *node);
```

## 算子类型

### 扫描算子
- SeqScan: 顺序扫描
- IndexScan: 索引扫描
- IndexOnlyScan: 仅索引扫描
- BitmapScan: 位图扫描

### 连接算子
- HashJoin: Hash 连接
- NestLoop: 嵌套循环
- MergeJoin: 归并连接

### 聚合算子
- HashAgg: Hash 聚合
- SortAgg: 排序聚合

### 修改算子
- Insert
- Update
- Delete

## 面试高频题

1. 火山模型的优缺点
2. HashJoin 的实现原理
3. 聚合算子如何处理大数据量