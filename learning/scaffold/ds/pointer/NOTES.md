# 工程对照笔记

## 概述

本学习卡片演示的指针概念直接对应工程代码中的实际应用。以下对照工程源码中的指针使用模式，帮助理解理论与实践的关联。

---

## 对照一：链表节点与指针

**源文件**：`engineering/src/algo-prod/list/list.c`

链表是最经典的指针应用，每个节点包含指向下一个节点的指针：

```c
// list.c 中的链表节点结构
struct list_node {
    struct list_node *prev;   // 指向前驱节点
    struct list_node *next;   // 指向后继节点
    unsigned char data[];     // 可变长数据
};
```

**对照学习点**：
- `node->next` 是结构体指针成员访问
- `malloc(sizeof(list_node_t) + element_size)` 为数据和节点分配连续内存
- 指针的指针：`list_t` 中 `head` 和 `tail` 都是指针变量

---

## 对照二：迭代器与指针遍历

**源文件**：`engineering/src/algo-prod/list/list.c`

链表迭代器本质上是一个指针遍历结构：

```c
// 链表迭代器模式
typedef struct {
    list_t *list;
    list_node_t *current;
    size_t index;
} list_iter_t;

// 迭代器前进
static list_iter_t *list_iter_next(list_iter_t *iter)
{
    if (!iter || !iter->current) {
        return NULL;
    }
    iter->current = iter->current->next;  // 指针移动
    iter->index++;
    return iter;
}
```

**对照学习点**：
- 迭代器是结构体，包含当前节点的指针和索引
- `iter->current->next` 是嵌套指针解引用
- 指针算术在链表遍历中的应用

---

## 对照三：函数指针用于自定义释放

**源文件**：`engineering/src/algo-prod/list/list.c`

链表支持自定义元素释放函数，体现函数指针的回调模式：

```c
// list_t 结构中的函数指针
struct list {
    size_t element_size;
    size_t size;
    list_node_t *head;
    list_node_t *tail;
    void (*free_element)(void *element);  // 函数指针
};

// 创建时注册释放函数
list_t *list_create_ex(size_t element_size, void (*free_element)(void *))
{
    list_t *list = calloc(1, sizeof(list_t));
    list->element_size = element_size;
    list->free_element = free_element;  // 函数指针赋值
    return list;
}

// 销毁时通过函数指针回调
static void list_node_destroy(list_t *list, list_node_t *node, int transfer, void *out)
{
    if (!transfer && list->free_element) {
        list->free_element(node->data);  // 回调用户函数
    }
    free(node);
}
```

**对照学习点**：
- `void (*free_element)(void *)` 是函数指针类型
- 可以传入 `free`、自定义释放函数或 NULL
- 函数指针让链表支持多种资源管理策略

---

## 对照四：Buffer Pool 的 Hash 表查找

**源文件**：`engineering/src/db/storage/bufmgr.c`

Buffer Pool 使用指针数组管理页面描述符：

```c
// 页面描述符数组（指针数组）
typedef struct {
    PageDescriptor *pages;  // 指向页面描述符数组的指针
    size_t num_pages;
} BufferPool;

// Hash 表查找返回指向描述符的指针
PageDescriptor *buf_lookup(BufferPool *pool, PageId page_id)
{
    size_t hash = hash_page_id(page_id);
    // 遍历 hash 槽位，返回匹配项的指针
    for (int i = 0; i < HASH_BUCKET_SIZE; i++) {
        PageDescriptor **slot = &pool->hash_table[hash + i];
        if (*slot && (*slot)->page_id == page_id) {
            return *slot;  // 返回指向匹配项的指针
        }
    }
    return NULL;
}
```

**对照学习点**：
- `PageDescriptor **slot` 是指向指针的指针（二级指针）
- `*slot` 是 `PageDescriptor *`，`**slot` 是 `PageDescriptor`
- 指针的多层解引用在 hash 表查找中的应用

---

## 工程代码模式总结

| 模式 | 场景 | 工程应用 |
|------|------|----------|
| 指针遍历 | 链表、树、图 | `list.c`, `btreeam.c` |
| 函数指针 | 策略模式、回调 | `list.c` 释放函数, `qsort` 比较函数 |
| 指向指针的指针 | 输出参数、二级指针 | Hash 表查找, 文件指针 `FILE **` |
| 指针算术 | 数组操作 | Buffer Pool 页面数组, WAL 日志缓冲区 |
| const 指针 | API 参数保护 | 只读访问接口 |

---

## 延伸阅读

- `engineering/src/algo-prod/list/list.c` - 链表完整实现（函数指针回调）
- `engineering/src/db/storage/bufmgr.c` - Buffer Pool 实现（Hash 表与指针数组）
- `engineering/include/db/rel.h` - Relation 扫描接口（迭代器模式）
- `engineering/src/db/storage/heapam.c` - Heap 访问方法（页面指针操作）
