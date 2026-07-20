# Turso 与项目关联

## 学习目标
- 理解 Turso 边缘分发设计对项目的启发
- 掌握 libSQL 扩展机制在项目中的应用
- 对比 Turso 与项目存储引擎的架构差异

## 边缘分发设计启发

### 项目边缘分发架构设计

```c
// 项目边缘分发设计（规划）
typedef struct edge_replica {
    buffer_pool_t *local_cache;
    page_t *pages;
    uint64_t last_lsn;
    sync_queue_t *write_queue;
    char *remote_url;
    connection_t *conn;
} edge_replica_t;
```

### 读写分离架构对比

| 特性 | Turso | 项目规划 |
|------|-------|----------|
| 本地缓存 | SQLite 文件 | Buffer Pool |
| 写入转发 | HTTP 协议 | 自定义协议 |
| 同步机制 | WAL 帧同步 | LSN 同步 |
| 冲突解决 | 最后写入胜出 | MVCC |

## libSQL 扩展机制应用

### 项目向量扩展

```c
// 项目 SIMD 向量距离计算
float cosine_distance_f32_simd(const float *a, const float *b, int dim) {
    __m512 dot_vec = _mm512_setzero_ps();
    for (int i = 0; i < dim; i += 16) {
        __m512 va = _mm512_loadu_ps(a + i);
        __m512 vb = _mm512_loadu_ps(b + i);
        dot_vec = _mm512_fmadd_ps(va, vb, dot_vec);
    }
    return 1.0f - reduce_sum(dot_vec) / norm;
}
```

### ALTER TABLE 增强借鉴

```c
int alter_rename_column(catalog_t *cat, const char *table,
                        const char *old_name, const char *new_name) {
    column_t *col = catalog_find_column(cat, table, old_name);
    if (col == NULL) return -1;
    catalog_update_column_name(cat, col->oid, new_name);
    return 0;
}
```

## 存储引擎架构对比

### VDBE vs 项目执行器

| 特性 | VDBE（字节码） | 项目执行器（树形） |
|------|---------------|------------------|
| 执行模式 | 解释执行字节码 | 递归遍历执行树 |
| 优化机会 | 编译时优化 | 运行时优化 |
| 调试难度 | 较难 | 较易 |
| 性能 | 高度优化 | 可优化空间大 |

## 嵌入式副本模式实践

### 离线优先架构设计

```c
typedef struct embedded_replica {
    kv_t *local_db;
    buffer_pool_t *cache;
    uint64_t last_lsn;
    xlog_t *write_queue;
    char *remote_url;
    conn_t *conn;
} embedded_replica_t;

int replica_write(embedded_replica_t *rep, const char *key,
                  const void *val, size_t len) {
    int rc = kv_put(rep->local_db, key, val, len);
    if (rc != 0) return rc;
    xlog_entry_t entry = { XLOG_WRITE, key, val, len, time(NULL) };
    xlog_append(rep->write_queue, &entry);
    if (rep->conn && conn_is_connected(rep->conn)) {
        replica_sync(rep);
    }
    return 0;
}

int replica_sync(embedded_replica_t *rep) {
    xlog_iter_t *iter = xlog_begin_iter(rep->write_queue, rep->last_lsn);
    xlog_entry_t entry;
    while (xlog_next(iter, &entry)) {
        int rc = remote_write(rep->conn, entry.key, entry.value, entry.len);
        if (rc != 0) { xlog_end_iter(iter); return rc; }
        rep->last_lsn = entry.lsn;
    }
    xlog_end_iter(iter);
    xlog_truncate(rep->write_queue, rep->last_lsn);
    return 0;
}
```

### 冲突解决策略

| 策略 | 说明 | 适用场景 |
|------|------|----------|
| LWW | 最后写入胜出 | 简单场景 |
| MERGE | 自动合并 | 可合并数据 |
| ERROR | 拒绝写入 | 强一致性 |
| CUSTOM | 自定义处理 | 复杂业务 |

## 要点总结

- 边缘分发设计：借鉴嵌入式副本模式，实现离线优先架构
- 向量扩展机制：项目已实现 SIMD 优化
- VDBE vs 执行器：字节码 vs 树形执行器
- 冲突解决策略：支持 LWW / 合并 / 自定义

## 思考题

1. 项目边缘分发设计如何与 Buffer Pool 集成？
2. VDBE 字节码执行在查询优化上有何优势？
3. 嵌入式副本冲突解决策略如何选择？
