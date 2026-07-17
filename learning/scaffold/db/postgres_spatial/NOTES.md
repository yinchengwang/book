# 工程对照笔记

## 工程源码位置
`engineering/src/db/index/rtree/`

## 数据结构对照

| 教学代码 | 工程代码 | 说明 |
|----------|----------|------|
| `mbr_t` | `rect_t` | 同为矩形结构，成员名不同但含义一致 |
| `rtree_node_t` (单文件) | `rtree_node_t` + `rtree_entry_t` | 工程版将条目抽取为独立结构 |
| `child_mbrs[]` + `u.children[]` 分离 | `mbr` (节点级) + `u.children[]` | 工程版节点自带一个 `mbr`，孩子数组独立 |
| `MAX_ENTRIES = 4` | `RTREE_DEFAULT_MAX_ENTRIES = 9` | 工程版默认条目数更大 |

## 函数对照

### 矩形操作
- `mbr_area()` — 教学用，工程版未单独暴露（内联计算）
- `mbr_union()` — 对应 `_rect_union()`，实现逻辑一致
- `mbr_intersects()` — 对应 `_rect_intersects()`，工程版命名加下划线前缀

### 节点管理
- `node_create()` — 对应 `_rtree_node_create()`，工程版需传入 `rtree_t *` 获取 `max_entries`
- 教学版 `node_split()` 简化按 x 轴排序分裂；工程版注释提到"允许临时溢出"但未实现完整分裂

### 插入
- `rtree_insert_internal()` — 对应 `_rtree_node_insert()`
- 教学版实现了完整分裂 + 新根创建；工程版简化处理"节点满了直接添加允许溢出"
- 教学版最小面积增长算法与工程版一致

### 搜索
- `rtree_search()` — 对应 `_rtree_node_search()`
- 回调函数模式：工程版用 `rtree_callback_fn`，教学版直接填充结果数组
- 剪枝逻辑一致：MBR 不相交则跳过

## 工程版特点

1. **模块化拆分**：`rtree_core.c` / `rtree_insert.c` / `rtree_search.c` / `rtree_delete.c`
   - 教学版全部在 `main.c` 中便于理解整体流程

2. **头文件设计**：
   - 公共 API：`engineering/include/db/index/rtree/rtree.h`
   - 内部结构：`rtree_private.h`
   - 教学版无头文件分离，直接定义

3. **删除功能**：工程版 `rtree_delete.c` 实现了简化删除，教学版未实现

4. **节点结构**：
   ```c
   // 工程版
   typedef struct {
       rect_t rect;
       void *data;
   } rtree_entry_t;
   
   typedef struct rtree_node {
       bool is_leaf;
       uint32_t nentries;
       uint32_t max_entries;
       rect_t mbr;  // 节点级 MBR
       union {
           rtree_entry_t *entries;       // 动态数组
           struct rtree_node **children; // 动态数组
       } u;
   } rtree_node_t;
   ```
   
   ```c
   // 教学版
   typedef struct rtree_node {
       bool is_leaf;
       int nentries;
       mbr_t mbr;
       mbr_t child_mbrs[MAX_ENTRIES + 1];  // 固定数组
       union {
           void *data[MAX_ENTRIES + 1];
           struct rtree_node *children[MAX_ENTRIES + 1];
       } u;
   } rtree_node_t;
   ```

5. **内存管理**：工程版使用动态数组 + `calloc`，教学版用固定数组简化

## 差异总结

| 方面 | 教学版 | 工程版 |
|------|--------|--------|
| 分裂算法 | 按 x 中心排序均分 | 未完整实现（允许溢出） |
| 条目存储 | 固定数组 | 动态数组 |
| 回调机制 | 无 | 支持回调遍历 |
| 删除操作 | 无 | 简化实现 |
| max_entries | 硬编码 4 | 可配置，默认 9 |
| 头文件 | 单文件 | 公共/私有分离 |

## 学习要点

工程版 R-Tree 侧重生产可用性：
- 动态内存适配不同配置
- 回调机制支持灵活查询
- 模块化便于维护

教学版侧重演示核心原理：
- 固定小容量便于调试
- 完整分裂流程展示算法细节
- 单文件易于理解全貌