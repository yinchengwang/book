## Context

项目已有 B-Tree 和 B+Tree 两种经典树索引，采用统一代码结构：
- 每个索引有 `core`、`insert`、`delete`、`lookup`、`persist` 模块
- 私有头文件定义数据结构
- 公共头文件暴露 API
- 支持自定义比较函数

本次新增 5 种树索引，需要保持代码风格一致性，同时适配各索引的特性。

## Goals / Non-Goals

**Goals:**
- 新增 5 种树索引：T-Tree、Skip List、Radix Tree、R-Tree、ART
- 保持与现有 B-Tree/B+Tree 一致的代码结构和命名风格
- 每种索引包含核心操作（创建、销毁、插入、删除、查找）
- 统一的模块划分和头文件组织

**Non-Goals:**
- 不实现持久化（persist 模块可空实现，后续按需扩展）
- 不实现并发控制（单线程版本）
- R-Tree 暂不实现复杂空间查询（如最近邻）
- ART 暂不实现自适应节点压缩

## Decisions

### Decision 1: T-Tree 实现策略

**选择**：参考 TimesTen 原始论文，实现简化版 T-Tree

**理由**：
- T-Tree 每个节点存储 `[min_keys, max_keys]` 个 key（max = 2*min-1）
- 节点既可以是叶子也可以是内部节点
- 支持左右子树指针和兄弟链表

**替代方案**：
- 参考现有实现（B-Tree/B+Tree），但 T-Tree 的旋转/分裂逻辑有独特之处
- 最终选择：实现标准 T-Tree 操作，保持与现有代码风格一致

```
节点结构：
┌─────────────────────────────────────────┐
│ is_leaf | nkeys | keys[] | values[]     │
│         | left   | right | prev | next  │
└─────────────────────────────────────────┘
```

### Decision 2: Skip List 实现策略

**选择**：标准 Skip List + 扩展范围查询

**理由**：
- Skip List 使用概率平衡，无需再平衡操作
- 实现简单，性能可预测
- Redis ZSet 使用类似结构

**层级策略**：
- 最大层级：16
- 新节点晋升概率：0.5
- 从最高层往下搜索

### Decision 3: Radix Tree 实现策略

**选择**：压缩前缀树（Compact Radix Tree）

**理由**：
- 相同前缀的边合并，减少空间
- 支持最长前缀匹配
- Redis SDS 使用类似结构

**节点类型**：
- `radix_node_t`：普通节点，含子节点映射
- 压缩存储：节点存储 `[prefix, prefix_len]`

### Decision 4: R-Tree 实现策略

**选择**：R-Tree 的简化版（类 Guttman R-Tree）

**理由**：
- R-Tree 将矩形分组存储，递归组织
- 查询时使用 MBR（最小边界矩形）剪枝
- 简化版实现 insert（含节点分裂）和 search

**矩形定义**：
```c
typedef struct {
    float min_x, min_y;
    float max_x, max_y;
} rect_t;
```

### Decision 5: ART 实现策略

**选择**：ART 的简化版（Adaptive Radix Tree）

**理由**：
- ART 根据 key 前缀长度自适应选择节点类型
- 4 种节点类型：N4/N16/N48/N256
- 空间利用率高，查询性能接近二叉搜索树

**节点类型**：
| 类型 | 子节点数 | 适用场景 |
|------|----------|----------|
| N4   | 4        | 前 4 位唯一 |
| N16  | 16       | 前 8 位唯一 |
| N48  | 48       | 前 16 位唯一 |
| N256 | 256      | 前 32 位唯一 |

### Decision 6: 目录结构

每个索引目录结构：
```
ttree/
├── CMakeLists.txt
├── ttree_core.c       # 创建/销毁/比较
├── ttree_insert.c     # 插入
├── ttree_delete.c     # 删除
├── ttree_lookup.c     # 查找
└── ttree_private.h    # 私有定义

include/index/tree_index/
├── tree_index.h       # 统一头文件（可选）
├── ttree.h            # 公共 API
├── skip_list.h
├── radix_tree.h
├── rtree.h
└── art.h
```

## Risks / Trade-offs

| 风险 | 描述 | 缓解措施 |
|------|------|----------|
| T-Tree 旋转逻辑复杂 | T-Tree 的借位和合并比 B-Tree 复杂 | 先实现核心逻辑，旋转优化后续 |
| R-Tree 分裂算法 | R-Tree 分裂有多种策略（Quadratic/Linear） | 简化版使用 Linear 分裂 |
| ART 节点类型切换 | 节点类型随 key 增长需动态切换 | 统一节点结构，简化类型切换 |

## Open Questions

1. **T-Tree 是否需要支持持久化？** 暂不支持，后续可扩展
2. **Skip List 层级最大值设定？** 暂定 16，可按需调整
3. **R-Tree 是否需要支持 3D/高维？** 暂只实现 2D 版本
4. **ART 的 key 类型？** 暂定固定长度 binary key，方便实现