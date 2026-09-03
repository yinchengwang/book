# 工程对照说明

## 贪心算法在工程中的实际应用

### 1. 任务调度 (Task Scheduling)

操作系统和分布式系统中的任务调度广泛使用贪心策略：

#### 优先级调度 (Priority Scheduling)
```c
/* task_scheduler.c - 优先级队列调度 */
typedef struct {
    int task_id;
    int priority;      /* 优先级（数值越小越高） */
    int burst_time;    /* 执行时间 */
    time_t arrival;   /* 到达时间 */
} task_t;

/* 按优先级和到达时间贪心选择 */
task_t* select_next_task(priority_queue_t *pq, time_t current_time) {
    task_t *best = NULL;
    for (int i = 0; i < pq->size; i++) {
        if (pq->tasks[i].arrival <= current_time) {
            if (!best || pq->tasks[i].priority < best->priority) {
                best = &pq->tasks[i];
            }
        }
    }
    return best;
}
```

这与本模块活动选择的贪心策略本质相同：按某种度量（优先级/结束时间）选择最优任务。

#### 多级反馈队列 (MLFQ)
Linux CFS 调度器使用的完全公平调度，隐含贪心思想：
- CPU 密集型任务被「惩罚」降级
- I/O 密集型任务被「奖励」提升

### 2. 压缩算法 (Compression)

#### Huffman 编码在工程中的应用
- **gzip/zip**: DEFLATE 算法结合 LZ77 + Huffman
- **JPEG**: 量化后的 AC/DC 系数使用 Huffman 编码
- **MP3/AAC**: 音频帧使用 Huffman 编码

```c
/* 工程中的 Huffman 编码实现片段 */
static void build_huffman_table(huffman_encoder_t *enc, frequency_t *freq) {
    /* 使用最小堆构建 Huffman 树 */
    min_heap_t *heap = min_heap_create(freq, n_symbols);
    while (heap->size > 1) {
        node_t *left = min_heap_extract_min(heap);
        node_t *right = min_heap_extract_min(heap);
        node_t *parent = node_create('\0', left->freq + right->freq);
        parent->left = left;
        parent->right = right;
        min_heap_insert(heap, parent);
    }
    /* 生成编码表 */
    generate_codes(heap->root, "", enc->codes);
}
```

#### LZ77 滑动窗口 (类似贪心的最长匹配)
```c
/* lz77_encoder.c - 贪心寻找最长匹配 */
offset_t find_longest_match(lz77_state_t *state, int lookahead_start) {
    offset_t best_offset = 0, best_length = 0;
    int search_start = state->window_start;

    /* 贪心：从最近位置开始找最长匹配 */
    for (int i = search_start; i < state->lookahead_start; i++) {
        int len = 0;
        while (len < state->lookahead_len &&
               state->window[i + len] == state->lookahead[lookahead_start + len]) {
            len++;
        }
        if (len > best_length) {
            best_length = len;
            best_offset = state->lookahead_start - i;
        }
    }
    return (offset_t){best_offset, best_length};
}
```

### 3. 缓存置换算法 (Cache Replacement)

#### LRU-K 和 ARC 的贪心近似
虽然严格 LRU 需要精确历史，但近似实现使用贪心策略：

```c
/* cache_arc.c - Adaptive Replacement Cache */
typedef struct {
    list_t t1;   /* 频繁访问的最近访问 */
    list_t t2;   /* 频繁访问的历史访问 */
    list_t b1;   /* t1 的历史副本 */
    list_t b2;   /* t2 的历史副本 */
    size_t p;    /* 目标大小（动态调整） */
} arc_cache_t;

/* 贪心调整 p 以平衡命中率和污染 */
void arc_adapt(arc_cache_t *cache, int hit_in_t1, int hit_in_t2) {
    if (hit_in_t1) {
        /* 命中 t1：向左扩展，增加短期项 */
        cache->p = cache->p > 0 ? cache->p - 1 : 0;
    } else if (hit_in_t2) {
        /* 命中 t2：向右扩展，增加长期项 */
        cache->p = cache->p < cache->max_size ? cache->p + 1 : cache->max_size;
    }
    /* 贪心调整，直到平衡 */
    arc_rebalance(cache);
}
```

### 4. 图算法中的贪心

#### Prim 最小生成树
```c
/* prim_mst.c - 贪心扩展切分 */
void prim_mst(graph_t *g, edge_t *mst_edges, int *mst_size) {
    bool visited[MAX_VERTICES] = {false};
    priority_queue_t pq;  /* (weight, vertex) */

    visited[0] = true;
    for (all edges from 0) pq_push(&pq, edge);

    while (!pq_empty(&pq) && *mst_size < g->n - 1) {
        edge_t e = pq_pop_min(&pq);
        if (visited[e.to]) continue;  /* 跳过已访问 */

        visited[e.to] = true;
        mst_edges[(*mst_size)++] = e;

        /* 贪心：添加所有从新顶点出发的最小边 */
        for (all edges from e.to) {
            if (!visited[edge.to]) {
                pq_push(&pq, edge);
            }
        }
    }
}
```

### 学习路径

- 本 scaffold (learning/scaffold/ds/greedy/) → 理解贪心核心思想
- 深入图算法 (learning/scaffold/ds/graph_repr/, graph_shortest_path/) → 贪心在图中的应用
- 工程生产 (engineering/src/algo-prod/compression/, scheduler/) → 真实压缩器和调度器
