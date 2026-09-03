# 网络拓扑设计 学习笔记

## 核心概念

- **星型拓扑**: 中心节点连接所有设备，布线简单但中心是 SPOF
- **环形拓扑**: 节点首尾相连，双环可容错，扩展受限
- **全网状拓扑**: 所有节点互连，容错最高但成本 O(n²)
- **部分网状**: 仅关键节点互连，在成本和容错间权衡

## 拓扑对比

| 拓扑 | 连接数 | 容错性 | 扩展性 | 成本 |
|------|--------|--------|--------|------|
| 星型 | O(n) | 差 | 好 | 低 |
| 环形 | O(n) | 中 | 中 | 低 |
| 全网状 | O(n²) | 极好 | 差 | 极高 |
| 部分网状 | O(n log n) | 好 | 中 | 中 |

## 工程对照

`engineering/` 轨中的 CMake 库依赖关系（`index → algo → project_includes`）形成了一种"有向无环图 (DAG)"结构，这与网络拓扑中的"部分网状拓扑"类似——关键模块间直接相连，非关键模块通过中间层间接通信。`engineering/src/db/txn/txn.c` 中的事务管理涉及多模块协调（Lock Manager → Buffer Manager → WAL Manager → Storage Engine），形成了"星型+网状"的混合拓扑——事务管理作为中心节点协调其他模块（星型），而 Buffer Pool 与 WAL 之间还有直接交互（网状）。`engineering/apps/web/` 中的前端应用（6 个 HTML 页面）通过 `nav-component.js` 统一导航相互连接，这类似于"星型拓扑"——导航组件作为中心，各页面作为叶子节点。`engineering/include/db/mm_storage.h` 中的多模态存储引擎（KV/Vector/Document/Spatial/Graph）共用 Buffer Pool，这是"星型拓扑"在存储层的体现。

## 面试要点

1. 生产环境使用混合拓扑：数据中心内 Spine-Leaf 架构
2. 负载均衡器本身也是 SPOF——需主备部署
3. 拓扑选择需权衡：成本、容错、性能、可管理性
