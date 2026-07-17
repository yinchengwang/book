# 工程对照笔记

## 概述

本学习卡片演示的函数指针概念直接对应工程代码中的实际应用。以下对照工程源码中的回调和比较器使用模式，帮助理解理论与实践的关联。

---

## 对照一：BTree 比较器函数

**源文件**：`engineering/src/db/index/btree/btree_persist.c`

BTree 索引在创建时接受一个比较器函数，实现自定义键类型的排序：

```c
// btree_persist.c 中的比较器函数类型
typedef btree_key_t (*btree_compare_fn)(const void *key1, const void *key2, void *ctx);
typedef void *(*btree_compare_ctx);

// 创建 BTree 索引时注册比较器
btree_index_t *btree_index_create(
    int min_degree,
    btree_compare_fn compare,      /* 比较器函数指针 */
    void *compare_ctx              /* 比较器上下文 */
);

// 实际查找时调用比较器
btree_key_t btree_compare_keys(btree_index_t *idx, const void *key1, const void *key2)
{
    return idx->compare(key1, key2, idx->compare_ctx);  /* 回调比较器 */
}
```

**对照学习点**：
- 比较器函数指针允许 BTree 支持任意键类型（int、string、复合键等）
- `compare_ctx` 参数允许比较器持有额外状态（如大小写不敏感的字符串比较器）
- 这是策略模式在索引实现中的典型应用

---

## 对照二：优化器成本估算排序

**源文件**：`engineering/src/db/optimizer/optimizer_cost.c`

优化器对多个执行计划按成本排序时使用比较器：

```c
// optimizer_cost.c 中的成本比较函数
int cost_compare(const cost_estimate_t *a, const cost_estimate_t *b)
{
    // 比较总成本，返回 -1/0/1
    if (a->total_cost < b->total_cost) return -1;
    if (a->total_cost > b->total_cost) return 1;
    return 0;
}

// 使用 qsort 对候选计划排序
qsort(candidates, num_candidates, sizeof(cost_estimate_t), cost_compare);
```

**对照学习点**：
- 优化器候选计划需要按成本排序选择最优执行计划
- 比较器函数使排序逻辑与数据结构分离
- 可以轻松改为按启动时间、内存使用等不同指标排序

---

## 对照三：qsort 在成本排序中的应用

**源文件**：`engineering/src/db/optimizer/optimizer_cost.c`

优化器对候选计划排序以选择最优方案：

```c
// 对执行计划按成本排序
static void sort_candidate_plans(cost_estimate_t *plans, size_t n)
{
    qsort(plans, n, sizeof(cost_estimate_t),
          (int (*)(const void *, const void *))cost_compare);
}
```

**对照学习点**：
- qsort 是标准库的通用排序函数，接受比较器作为参数
- 传入 `sizeof(element)` 和比较函数指针即可排序任意类型数组
- 配合强制类型转换，可以排序结构体数组

---

## 对照四：回调函数在其他系统中的应用

**源文件**：`engineering/src/algo-prod/list/list.c`

链表支持自定义元素释放函数，体现函数指针的灵活性：

```c
// list.c 中的函数指针回调
typedef void (*free_element_fn)(void *element);

// 创建时传入释放函数
list_t *list_create_ex(size_t element_size, free_element_fn free_element)
{
    list_t *list = calloc(1, sizeof(list_t));
    list->free_element = free_element;  /* 保存回调函数 */
    return list;
}

// 销毁链表时调用用户提供的释放函数
void list_clear(list_t *list)
{
    list_node_t *current = list->head;
    while (current) {
        list_node_t *next = current->next;
        if (list->free_element) {
            list->free_element(current->data);  /* 回调用户函数 */
        }
        free(current);
        current = next;
    }
    // ...
}
```

**对照学习点**：
- 链表不直接包含资源释放逻辑，而是通过回调函数委托给用户
- 同一个链表可以用于管理不同类型的资源（只需提供对应的释放函数）
- 这是"控制反转"（IoC）思想的体现

---

## 工程代码模式总结

| 模式 | 场景 | 工程应用 |
|------|------|----------|
| 比较器回调 | 排序、查找、二叉搜索树 | `qsort`, `bsearch`, `btree_persist.c` |
| 策略模式 | 可替换算法 | `optimizer_cost.c` 成本模型切换 |
| 回调函数 | 生命周期钩子 | `list.c` 自定义释放函数 |
| 上下文参数 | 状态持有 | BTree 比较器 `compare_ctx` |

---

## 延伸阅读

- `engineering/src/db/index/btree/btree_persist.c` - BTree 索引实现（比较器函数指针）
- `engineering/src/db/optimizer/optimizer_cost.c` - 优化器成本估算（qsort 比较器）
- `engineering/src/algo-prod/list/list.c` - 链表实现（函数指针回调）
- `engineering/src/db/utils/faiss_heap.c` - Faiss 堆操作（比较器回调）
