# P1-4 图分析算法库 提案

## 背景

多模态能力补齐系列中，P1-4 图分析算法库是剩余 6 个待完成任务之一。当前图存储引擎已实现基本的顶点/边操作和 Cypher 查询语言解析，但缺少常用的图分析算法（如最短路径、PageRank、社区检测等）。

## 变更范围

### 新增文件
| 文件 | 说明 |
|------|------|
| `engineering/include/db/graph/graph_algorithms.h` | 图分析算法公共 API |
| `engineering/src/db/graph/graph_algorithms.c` | 算法实现 |
| `engineering/test/db/graph/graph_algorithms_test.cpp` | GoogleTest 测试 |

### 修改文件
| 文件 | 说明 |
|------|------|
| `engineering/src/db/graph/CMakeLists.txt` | 注册新模块 |

## 核心功能

1. **最短路径算法**
   - Dijkstra 单源最短路径
   - BFS 无权图最短路径
   - A* 启发式搜索（可选）

2. **图遍历算法**
   - DFS 深度优先遍历
   - BFS 广度优先遍历

3. **中心性算法**
   - PageRank（迭代计算）
   - 度中心性（Degree Centrality）
   - 介数中心性（Betweenness Centrality，可选）

4. **社区检测**
   - 连通分量（Connected Components）
   - Louvain 社区检测（可选）

5. **图统计**
   - 顶点数/边数
   - 平均度
   - 聚类系数

## 设计约束

- C11 标准，保持与现有图存储引擎的 ABI 兼容
- 算法函数接受 `graph_engine_t*` 作为输入，不修改图结构（只读算法）
- 返回结果通过输出参数传递，调用者负责释放
- 所有算法必须有对应的 GoogleTest 测试用例

## 验收标准

- [ ] Dijkstra 最短路径测试通过
- [ ] BFS 遍历测试通过
- [ ] PageRank 迭代收敛测试通过
- [ ] 连通分量检测测试通过
- [ ] 图统计函数返回正确结果
- [ ] 所有测试在 `ctest --test-dir build/engineering -R graph_algorithms` 下通过

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| 图算法复杂度高 | 优先实现 Dijkstra + BFS + PageRank，其他算法可选 |
| 与现有图引擎集成 | 通过 `graph_engine.h` 提供的接口访问图数据 |
| 测试数据构造 | 使用小规模测试图（< 100 顶点）验证正确性 |