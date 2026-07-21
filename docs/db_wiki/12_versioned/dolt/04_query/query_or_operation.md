# Dolt 查询或操作引擎

## 学习目标

- 理解 Dolt 的 SQL 查询处理流程
- 掌握基于 Go MySQL Server 的执行器架构
- 了解版本控制操作的实现机制
- 对比分析 Dolt 执行引擎与本项目 algo/ 模块的关联

## 查询执行架构

```mermaid
graph TB
    subgraph "客户端层"
        MYSQL_CLI["MySQL 客户端"]
        DOLT_CLI["dolt CLI"]
        SQL_REPL["SQL REPL"]
    end

    subgraph "协议层"
        PGWIRE["MySQL Wire 协议"]
        AUTH["认证模块"]
    end

    subgraph "SQL 引擎层"
        PARSER["SQL 解析器"]
        ANALYZER["查询分析器"]
        PLANNER["查询计划器"]
        OPTIMIZER["查询优化器"]
    end

    subgraph "执行层"
        EXECUTOR["执行引擎"]
        NODES["执行节点<br/>Scan/Join/Agg/Sort"]
        EXPRESSION["表达式求值"]
    end

    subgraph "版本控制层"
        DOLT_COMMIT["dolt_commit()"]
        DOLT_BRANCH["dolt_branch()"]
        DOLT_MERGE["dolt_merge()"]
        DOLT_DIFF["dolt_diff()"]
    end

    subgraph "存储层"
        PROLLY["Prolly-Tree"]
        CHUNK["Chunk Store"]
    end

    MYSQL_CLI --> PGWIRE
    DOLT_CLI --> PARSER
    SQL_REPL --> PARSER
    PGWIRE --> AUTH
    AUTH --> PARSER
    PARSER --> ANALYZER
    ANALYZER --> PLANNER
    PLANNER --> OPTIMIZER
    OPTIMIZER --> EXECUTOR
    EXECUTOR --> NODES
    NODES --> EXPRESSION
    NODES --> PROLLY
    EXECUTOR --> DOLT_COMMIT
    EXECUTOR --> DOLT_BRANCH
    EXECUTOR --> DOLT_MERGE
    EXECUTOR --> DOLT_DIFF
    DOLT_COMMIT --> PROLLY
    PROLLY --> CHUNK
```

## SQL 查询执行流程

### 完整查询生命周期

```mermaid
sequenceDiagram
    participant C as 客户端
    participant W as Wire 协议
    participant P as 解析器
    participant A as 分析器
    participant L as 计划器
    participant O as 优化器
    participant E as 执行器
    participant S as 存储层

    C->>W: 发送 SQL 查询
    W->>W: 认证和验证
    W->>P: 传递 SQL 文本
    P->>P: 词法分析和语法解析
    P->>A: 返回 AST
    A->>A: 名称解析和类型检查
    A->>A: 绑定表/列元数据
    A->>L: 返回已解析 AST
    L->>L: 生成逻辑计划
    L->>L: 应用转换规则
    L->>O: 返回逻辑计划
    O->>O: 代价估算
    O->>O: 选择最优计划
    O->>E: 返回物理计划
    E->>E: 初始化执行节点树
    E->>S: 打开表/索引
    loop 迭代执行
        E->>S: 获取下一批元组
        S-->>E: 返回数据
        E->>E: 应用谓词/投影
    end
    E-->>W: 返回结果集
    W-->>C: 发送结果
```

### 核心执行节点

```mermaid
graph TB
    subgraph "执行节点树"
        ROOT["Result 节点<br/>根节点"]
        SCAN["SeqScan 节点<br/>全表扫描"]
        INDEX_SCAN["IndexScan 节点<br/>索引扫描"]
        JOIN["HashJoin 节点<br/>哈希连接"]
        AGG["HashAgg 节点<br/>哈希聚合"]
        SORT["Sort 节点<br/>排序"]
        LIMIT["Limit 节点<br/>限制"]
    end

    ROOT --> JOIN
    JOIN --> SCAN
    JOIN --> INDEX_SCAN
    SCAN --> AGG
    INDEX_SCAN --> AGG
    AGG --> SORT
    SORT --> LIMIT
```

### 执行节点类型

| 节点类型 | 功能 | 适用场景 |
|---------|------|---------|
| SeqScan | 全表扫描 | 无索引查询、小表 |
| IndexScan | 索引扫描 | 有索引的选择查询 |
| HashJoin | 哈希连接 | 等值连接 |
| MergeJoin | 归并连接 | 有序输入连接 |
| NestedLoop | 嵌套循环 | 小表驱动大表 |
| HashAgg | 哈希聚合 | 大数据量聚合 |
| SortAgg | 排序聚合 | 有序输入聚合 |
| Sort | 排序 | ORDER BY |
| Limit | 限制行数 | LIMIT 子句 |

## 版本控制操作引擎

### dolt_commit() 实现

```mermaid
flowchart TB
    START[调用 dolt_commit] --> CHECK{工作区干净?}
    CHECK -- 是 --> ERROR[错误: 无变更]
    CHECK -- 否 --> BUILD[构建 Commit 对象]
    BUILD --> TREE[生成 Prolly-Tree 快照]
    TREE --> HASH[计算根哈希]
    HASH --> PARENT[设置父 Commit]
    PARENT --> WRITE[写入 Commit 链]
    WRITE --> UPDATE[更新 HEAD 引用]
    UPDATE --> END[返回 Commit ID]
```

### dolt_branch() 实现

```mermaid
flowchart TB
    START[调用 dolt_branch] --> TYPE{操作类型}
    TYPE -- 创建 --> CREATE[创建新分支引用]
    CREATE --> COPY[复制当前 HEAD Commit]
    COPY --> SAVE[保存到 refs/heads/]
    SAVE --> END[分支创建成功]
    TYPE -- 列表 --> LIST[扫描 refs/heads/]
    LIST --> OUTPUT[输出分支列表]
    TYPE -- 删除 --> DELETE[删除分支引用]
    DELETE --> CHECK{有未合并提交?}
    CHECK -- 是 --> WARN[警告: 未合并提交]
    CHECK -- 否 --> DEL[移除引用文件]
    DEL --> END2[分支删除成功]
```

### dolt_merge() 实现

```mermaid
flowchart TB
    START[调用 dolt_merge] --> FIND[找到共同祖先]
    FIND --> DIFF1[计算源分支变更]
    DIFF1 --> DIFF2[计算目标分支变更]
    DIFF2 --> CONFLICT{有冲突?}
    CONFLICT -- 否 --> APPLY[应用变更]
    APPLY --> COMMIT[创建合并 Commit]
    COMMIT --> END[合并成功]
    CONFLICT -- 是 --> DETECT[检测冲突行]
    DETECT --> MARK[标记冲突]
    MARK --> WAIT[等待用户解决]
    WAIT --> RESOLVE[dolt_conflicts_resolve]
    RESOLVE --> APPLY
```

### dolt_diff() 实现

```mermaid
flowchart TB
    START[调用 dolt_diff] --> COMMIT1[获取 Commit1 树]
    COMMIT1 --> COMMIT2[获取 Commit2 树]
    COMMIT2 --> COMPARE[比较两棵 Prolly-Tree]
    COMPARE --> WALK[遍历叶子节点]
    WALK --> HASH_CMP{哈希相同?}
    HASH_CMP -- 是 --> SKIP[跳过: 无变更]
    HASH_CMP -- 否 --> DECODE[解码叶子节点]
    DECODE --> ROW_DIFF[行级 Diff 计算]
    ROW_DIFF --> OUTPUT[输出变更集]
```

## 核心算法和数据结构

### 查询优化算法

```go
// Go 伪代码：代价估算
func (p *Planner) estimateCost(plan PhysicalPlan) Cost {
    switch plan.Type {
    case SeqScan:
        return Cost{
            IO:  p.tableStats.RowCount * p.tableStats.AvgRowSize,
            CPU: p.tableStats.RowCount,
        }
    case IndexScan:
        selectivity := p.estimateSelectivity(plan.Filter)
        return Cost{
            IO:  p.tableStats.RowCount * selectivity,
            CPU: p.tableStats.RowCount * selectivity * 2,
        }
    case HashJoin:
        buildCost := p.estimateCost(plan.BuildSide)
        probeCost := p.estimateCost(plan.ProbeSide)
        return Cost{
            IO:  buildCost.IO + probeCost.IO,
            CPU: buildCost.CPU + probeCost.CPU + probeCost.RowCount,
        }
    }
}
```

### 哈希连接算法

```mermaid
sequenceDiagram
    participant E as 执行器
    participant B as 构建侧
    participant H as 哈希表
    participant P as 探测侧
    participant O as 输出

    E->>B: 扫描构建表
    loop 构建阶段
        B->>H: 插入连接键到哈希表
    end
    E->>P: 扫描探测表
    loop 探测阶段
        P->>H: 查找匹配键
        H-->>E: 返回匹配行
        E->>O: 输出连接结果
    end
```

### Prolly-Tree Diff 算法

```go
// Go 伪代码：Prolly-Tree Diff
func (t *ProllyTree) Diff(other *ProllyTree) []DiffEntry {
    var diffs []DiffEntry
    
    // 自顶向下比较
    if t.rootHash == other.rootHash {
        return diffs // 相同树，无变更
    }
    
    // 并行遍历两棵树
    t.walk(func(node *Node) {
        otherNode := other.findNode(node.Key)
        if otherNode == nil {
            diffs = append(diffs, DiffEntry{Op: INSERT, Key: node.Key, Val: node.Val})
        } else if node.Hash != otherNode.Hash {
            diffs = append(diffs, DiffEntry{Op: UPDATE, Key: node.Key, 
                                            OldVal: otherNode.Val, NewVal: node.Val})
        }
    })
    
    return diffs
}
```

## 与本项目 algo/ 模块的关联

### 架构对比

| 维度 | Dolt 执行引擎 | 本项目 algo 模块 |
|------|--------------|-----------------|
| 语言 | Go | C |
| 执行模型 | Volcano 迭代器 | Volcano 迭代器 |
| 节点注册 | 动态注册表 | 静态函数表 |
| 优化器 | 基于代价 | 基于规则 |
| 版本控制 | 原生集成 | 无 |

### 可借鉴的设计点

```mermaid
graph LR
    subgraph "Dolt 设计亮点"
        D1[版本控制集成]
        D2[动态节点注册]
        D3[内置函数扩展]
        D4[存储过程支持]
    end

    subgraph "项目可借鉴"
        P1[SQL 层版本控制]
        P2[插件化执行器]
        P3[用户定义函数]
        P4[存储过程框架]
    end

    D1 --> P1
    D2 --> P2
    D3 --> P3
    D4 --> P4
```

### 执行节点对应关系

| Dolt 节点 | 本项目节点 | 功能 |
|----------|-----------|------|
| executeSeqScan | ExecSeqScan | 全表扫描 |
| executeIndexScan | ExecIndexScan | 索引扫描 |
| executeHashJoin | ExecHashJoin | 哈希连接 |
| executeMergeJoin | ExecMergeJoin | 归并连接 |
| executeHashAgg | ExecAgg | 聚合 |
| executeSort | ExecSort | 排序 |
| executeLimit | ExecLimit | 限制 |

### 代码对应示例

```c
// 本项目: engineering/src/db/sql/executor.c
PlanState *ExecInitNode(Plan *plan, EState *estate, int eflags) {
    switch (plan->type) {
        case T_SeqScan:
            return ExecInitSeqScan((SeqScan *)plan, estate, eflags);
        case T_HashJoin:
            return ExecInitHashJoin((HashJoin *)plan, estate, eflags);
        case T_Agg:
            return ExecInitAgg((Agg *)plan, estate, eflags);
        // ...
    }
}
```

```go
// Dolt (Go MySQL Server): go/sqlexec/executor.go
func (e *Executor) Execute(ctx context.Context, query string) (Schema, RowIter, error) {
    plan, err := e.planner.QueryPlan(ctx, query)
    if err != nil {
        return nil, nil, err
    }
    return e.executePlan(ctx, plan)
}
```

## 要点总结

- **SQL 引擎**：基于 Go MySQL Server，支持 MySQL 协议
- **执行模型**：Volcano 迭代器模型，支持流水线执行
- **版本控制集成**：SQL 函数形式暴露（dolt_commit/branch/merge/diff）
- **优化器**：基于代价的优化，支持索引选择和连接顺序优化
- **对比本项目**：架构相似，但 Dolt 原生支持版本控制操作

## 思考题

1. Dolt 如何在 SQL 引擎中集成版本控制操作？这种设计有哪些优缺点？
2. 哈希连接在大数据量场景下的内存管理策略是什么？如何处理内存不足？
3. 本项目的 Volcano 执行器可以借鉴 Dolt 的哪些设计？动态节点注册是否适用？
4. 如何在本项目中实现 SQL 层的版本控制函数？需要修改哪些模块？
5. Dolt 的 Prolly-Tree Diff 算法的时间复杂度是多少？如何优化大规模数据集的 Diff？

## 参考资源

- [Dolt SQL Engine](https://www.dolthub.com/blog/2020-04-20-dolt-sql-engine/)
- [Go MySQL Server](https://github.com/dolthub/go-mysql-server)
- [Volcano 执行模型论文](https://paperhub.s3.amazonaws.com/dace52a42c07f7f8345b0862a7609875.pdf)
- 本项目: engineering/src/db/sql/executor.c