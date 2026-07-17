## Why

当前项目已有 B-Tree 和 B+Tree 两种经典树索引，但数据库领域还有多种针对不同场景优化的树索引：
- **T-Tree**：内存数据库专用索引（TimesTen、MySQL NDB Cluster）
- **Skip List**：并发友好索引（Redis ZSet、LevelDB MemTable）
- **Radix Tree**：前缀压缩索引（Redis SDS 大量使用）
- **R-Tree**：空间索引（PostGIS、地图应用）
- **ART**：自适应基数树（现代内存索引，论文常客）

这些索引在面试中经常出现，且能帮助理解不同存储引擎的设计取舍。

## What Changes

本次变更将新增 5 种树索引实现，全部采用与现有 B-Tree/B+Tree 一致的代码结构和风格：

1. **T-Tree**：`src/index/tree/ttree/`
   - 内存优化的多路搜索树
   - 单节点存储多个 key，平衡二叉查找树和 B-Tree 的优点

2. **Skip List**：`src/index/tree/skip_list/`
   - 层级链表结构，概率平衡
   - 实现 insert、delete、search、range query

3. **Radix Tree**：`src/index/tree/radix_tree/`
   - 前缀压缩的字典树变体
   - 高效前缀匹配和最长前缀查找

4. **R-Tree**：`src/index/tree/rtree/`
   - 空间数据索引（四边形/圆形区域）
   - 支持矩形查询、最近邻等空间操作

5. **ART**：`src/index/tree/art/`
   - 自适应基数树（Adaptive Radix Tree）
   - 根据 key 前缀长度自适应选择节点类型

## Capabilities

### New Capabilities

- `ttree-index`: T-Tree 内存索引实现，包含 core/insert/delete/lookup 四个核心模块
- `skip-list-index`: Skip List 并发友好索引实现
- `radix-tree-index`: Radix Tree 前缀压缩索引实现
- `rtree-index`: R-Tree 空间索引实现
- `art-index`: ART 自适应基数树实现

### Modified Capabilities

- 无

## Impact

- **新增目录**：`src/index/tree/ttree/`、`src/index/tree/skip_list/`、`src/index/tree/radix_tree/`、`src/index/tree/rtree/`、`src/index/tree/art/`
- **头文件**：`include/index/tree_index/` 下新增对应公共头文件
- **CMakeLists**：更新 `src/index/tree/CMakeLists.txt` 添加 5 个新子目录
- **测试**：在 `test/` 下新增对应测试文件
- **学习文档**：在 `notes/` 下新增 5 种索引的学习笔记