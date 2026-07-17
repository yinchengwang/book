# 图存储使用指南

## 概述

图存储引擎（Graph Engine）专为图数据的存储和遍历设计，支持 CSR 压缩存储格式和多种遍历算法。

## 核心特性

| 特性 | 说明 |
|------|------|
| CSR 存储 | 压缩稀疏行格式，O(1) 出入边查询 |
| COO 缓冲区 | 支持增量边添加 |
| 多种遍历 | BFS、DFS、最短路径、PageRank |
| 顶点标签 | 支持标签索引和过滤 |

## 快速开始

### 1. 初始化引擎

```c
#include "db/storage/graph/graph_engine.h"

// 初始化图引擎
graph_engine_init("/data/graphs");

// 创建图
graph_engine_create("social", 1000000);  // 最大顶点数
```

### 2. 打开图

```c
void *graph_db = graph_engine_open("social", ACCESS_MODE_READ_WRITE);
```

### 3. 添加顶点和边

```c
// 添加顶点
graph_vertex_id_t alice = graph_engine_add_vertex(graph_db, "Alice", NULL, 0);
graph_vertex_id_t bob = graph_engine_add_vertex(graph_db, "Bob", NULL, 0);
graph_vertex_id_t charlie = graph_engine_add_vertex(graph_db, "Charlie", NULL, 0);

printf("顶点 ID: Alice=%lu, Bob=%lu, Charlie=%lu\n", alice, bob, charlie);

// 添加边
graph_edge_id_t edge1 = graph_engine_add_edge(graph_db, alice, bob, "friend", 0);
graph_edge_id_t edge2 = graph_engine_add_edge(graph_db, bob, charlie, "friend", 0);
graph_edge_id_t edge3 = graph_engine_add_edge(graph_db, alice, charlie, "colleague", 0);

printf("添加了 %lu 条边\n", 3);
```

## CSR 存储

CSR（Compressed Sparse Row）提供高效的图存储，出入边查询时间复杂度为 O(1)。

### 启用 CSR

```c
// CSR 默认启用，可通过以下方式检查
graph_engine_db_t *db = (graph_engine_db_t *)graph_db;
if (db->use_csr) {
    printf("CSR 存储已启用\n");
}
```

### O(1) 出边查询

```c
// 获取顶点的所有出边
uint32_t out_count;
const graph_csr_edge_t *out_edges = graph_engine_get_out_edges(graph_db, alice, &out_count);

printf("Alice 的出边 (%u 条):\n", out_count);
for (uint32_t i = 0; i < out_count; i++) {
    printf("  -> %lu (类型: %s)\n", 
           out_edges[i].target, 
           out_edges[i].edge_type ? out_edges[i].edge_type : "unknown");
}
```

### O(1) 入边查询

```c
// 获取顶点的所有入边
uint32_t in_count;
const graph_csr_edge_t *in_edges = graph_engine_get_in_edges(graph_db, charlie, &in_count);

printf("Charlie 的入边 (%u 条):\n", in_count);
for (uint32_t i = 0; i < in_count; i++) {
    printf("  <- %lu (类型: %s)\n", 
           in_edges[i].source, 
           in_edges[i].edge_type ? in_edges[i].edge_type : "unknown");
}
```

### 合并 COO 到 CSR

```c
// 当 COO 缓冲区积累一定数据后，合并到 CSR
int ret = graph_engine_csr_compact(graph_db);
if (ret != 0) {
    fprintf(stderr, "CSR 合并失败\n");
}

// 获取 CSR 统计
uint64_t csr_vertices, csr_edges;
graph_engine_csr_stats(graph_db, &csr_vertices, &csr_edges);
printf("CSR: %lu 顶点, %lu 边\n", csr_vertices, csr_edges);
```

### 保存/加载 CSR

```c
// 保存 CSR 到文件
int ret = graph_engine_save_csr(graph_db);
if (ret != 0) {
    fprintf(stderr, "保存 CSR 失败\n");
}

// 加载 CSR
ret = graph_engine_load_csr(graph_db);
if (ret != 0) {
    fprintf(stderr, "加载 CSR 失败\n");
}
```

## BFS 遍历

广度优先搜索用于层次遍历图结构。

```c
// BFS 遍历
graph_engine_bfs_result_t bfs_result;
int ret = graph_engine_bfs(graph_db, alice, 3, &bfs_result);  // 深度 3

if (ret == 0) {
    printf("BFS 从 Alice 开始，深度 3:\n");
    printf("  访问了 %u 个顶点\n", bfs_result.num_vertices);
    
    for (uint32_t i = 0; i < bfs_result.num_vertices && i < 10; i++) {
        printf("  层级 %u: 顶点 %lu (距离=%u)\n",
               bfs_result.vertices[i].level,
               bfs_result.vertices[i].vertex_id,
               bfs_result.vertices[i].distance);
    }
    
    // 释放结果
    graph_engine_bfs_free(&bfs_result);
}
```

## DFS 遍历

深度优先搜索用于路径探索。

```c
// DFS 遍历
graph_engine_dfs_result_t dfs_result;
int ret = graph_engine_dfs(graph_db, alice, 10, &dfs_result);  // 最大 10 个顶点

if (ret == 0) {
    printf("DFS 从 Alice 开始:\n");
    printf("  访问了 %u 个顶点\n", dfs_result.num_vertices);
    
    for (uint32_t i = 0; i < dfs_result.num_vertices; i++) {
        printf("  [%u] 顶点 %lu\n", i, dfs_result.vertices[i]);
    }
    
    // 释放结果
    graph_engine_dfs_free(&dfs_result);
}
```

## 最短路径

Dijkstra 算法计算最短路径。

```c
// 计算最短路径
graph_path_result_t path;
int ret = graph_engine_shortest_path(graph_db, alice, charlie, &path);

if (ret == 0) {
    printf("Alice -> Charlie 最短路径:\n");
    printf("  路径长度: %u\n", path.length);
    printf("  路径: ");
    for (uint32_t i = 0; i < path.length; i++) {
        printf("%lu", path.path[i]);
        if (i < path.length - 1) printf(" -> ");
    }
    printf("\n");
    
    // 释放结果
    graph_engine_path_free(&path);
} else {
    printf("Alice 和 Charlie 之间没有路径\n");
}
```

## PageRank 算法

PageRank 计算图中顶点的重要性排名。

```c
// 计算 PageRank
graph_engine_pagerank_result_t pr;
int ret = graph_engine_pagerank(graph_db, 0.85, 100, 1e-6, &pr);

if (ret == 0) {
    printf("PageRank 结果 (top 10):\n");
    
    // 按排名排序（这里简化处理，实际需要排序）
    for (uint32_t i = 0; i < pr.num_vertices && i < 10; i++) {
        printf("  [%u] 顶点 %lu: %.6f\n",
               i, pr.ranks[i].vertex_id, pr.ranks[i].rank);
    }
    
    printf("迭代次数: %u\n", pr.iterations);
    printf("最终 delta: %.10f\n", pr.delta);
    
    // 释放结果
    graph_engine_pagerank_free(&pr);
}
```

## 顶点标签管理

```c
// 添加带标签的顶点
graph_engine_add_vertex_with_label(graph_db, "Alice", "user", NULL, 0);
graph_engine_add_vertex_with_label(graph_db, "Bob", "user", NULL, 0);
graph_engine_add_vertex_with_label(graph_db, "CompanyA", "organization", NULL, 0);

// 查询特定标签的顶点
graph_engine_db_t *db = (graph_engine_db_t *)graph_db;
if (db->csr_storage) {
    // 使用 CSR 标签索引查询
    // graph_csr_get_vertices_by_label(db->csr_storage, "user");
}
```

## 完整示例

```c
#include "db/storage/graph/graph_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    // 1. 初始化
    graph_engine_init("/tmp/graphs");
    graph_engine_create("social", 1000000);
    
    void *graph_db = graph_engine_open("social", ACCESS_MODE_READ_WRITE);
    
    // 2. 创建社交网络
    printf("创建社交网络...\n");
    
    // 添加用户
    graph_vertex_id_t users[5];
    const char *names[] = {"Alice", "Bob", "Charlie", "David", "Eve"};
    for (int i = 0; i < 5; i++) {
        users[i] = graph_engine_add_vertex(graph_db, names[i], NULL, 0);
        printf("  添加用户: %s (ID=%lu)\n", names[i], users[i]);
    }
    
    // 添加关系
    graph_engine_add_edge(graph_db, users[0], users[1], "friend", 0);  // Alice-Bob
    graph_engine_add_edge(graph_db, users[0], users[2], "friend", 0);  // Alice-Charlie
    graph_engine_add_edge(graph_db, users[1], users[2], "friend", 0);  // Bob-Charlie
    graph_engine_add_edge(graph_db, users[2], users[3], "friend", 0);  // Charlie-David
    graph_engine_add_edge(graph_db, users[3], users[4], "friend", 0);  // David-Eve
    graph_engine_add_edge(graph_db, users[4], users[0], "friend", 0);  // Eve-Alice
    
    // 3. CSR 查询测试
    printf("\nCSR 查询测试:\n");
    
    uint32_t out_count;
    const graph_csr_edge_t *out_edges = graph_engine_get_out_edges(
        graph_db, users[0], &out_count);
    printf("  Alice 出边数: %u\n", out_count);
    
    uint32_t in_count;
    const graph_csr_edge_t *in_edges = graph_engine_get_in_edges(
        graph_db, users[0], &in_count);
    printf("  Alice 入边数: %u\n", in_count);
    
    // 4. 合并 CSR
    printf("\n合并 CSR...\n");
    graph_engine_csr_compact(graph_db);
    
    // 5. 遍历算法
    printf("\nBFS 遍历:\n");
    graph_engine_bfs_result_t bfs;
    graph_engine_bfs(graph_db, users[0], 5, &bfs);
    printf("  访问 %u 个顶点\n", bfs.num_vertices);
    graph_engine_bfs_free(&bfs);
    
    printf("\n最短路径 Alice -> David:\n");
    graph_path_result_t path;
    if (graph_engine_shortest_path(graph_db, users[0], users[3], &path) == 0) {
        printf("  路径长度: %u\n", path.length);
        graph_engine_path_free(&path);
    }
    
    printf("\nPageRank:\n");
    graph_engine_pagerank_result_t pr;
    graph_engine_pagerank(graph_db, 0.85, 100, 1e-6, &pr);
    printf("  迭代次数: %u\n", pr.iterations);
    graph_engine_pagerank_free(&pr);
    
    // 6. 保存
    printf("\n保存图数据...\n");
    graph_engine_save_csr(graph_db);
    
    // 7. 清理
    graph_engine_close(graph_db);
    graph_engine_shutdown();
    
    return 0;
}
```

## 性能调优

### CSR vs COO

| 特性 | CSR | COO |
|------|-----|-----|
| 查询性能 | O(1) | O(n) |
| 插入性能 | O(n) | O(1) |
| 内存占用 | 低 | 高 |
| 适用场景 | 静态图、频繁查询 | 动态图、频繁插入 |

### 合并策略

建议在以下情况下合并 CSR：
- COO 缓冲区达到一定大小（建议 >10000 条边）
- 批量插入操作完成后
- 内存压力增大时
- 查询性能下降时

## 常见问题

**Q: CSR 和 COO 有什么区别？**
A: CSR 是压缩格式，查询效率高但插入效率低；COO 是增量格式，插入快但查询慢。两者结合使用。

**Q: 如何选择遍历算法？**
A: BFS 适合层次遍历和最短路径；DFS 适合路径探索和连通分量检测。

**Q: PageRank 的阻尼因子如何选择？**
A: 通常使用 0.85，值越大迭代越慢但精度越高。
