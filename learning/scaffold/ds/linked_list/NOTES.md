# 工程对照笔记

## 概述

本学习卡片演示的链表概念直接对应工程代码中的实际应用。以下对照工程源码中的链表实现，理解理论与实践的关联。

---

## 对照一：algo-prod 通用双向链表

**源文件**：`engineering/src/algo-prod/list/list.c`

工程中的链表是通用双向链表实现，支持任意类型元素的存储：

```c
/* 链表节点结构 - 使用灵活数组成员 */
struct list_node {
    struct list_node *prev;
    struct list_node *next;
    unsigned char data[];  /* 柔性数组，存储实际数据 */
};

/* 链表结构 */
struct list {
    size_t element_size;    /* 元素大小 */
    size_t size;            /* 元素数量 */
    list_node_t *head;      /* 头节点 */
    list_node_t *tail;      /* 尾节点 */
    void (*free_element)(void *element);  /* 元素释放回调 */
};
```

**对照学习点**：
- 使用 `unsigned char data[]` 柔性数组实现泛型
- `element_size` 记录元素大小，`malloc` 时分配 `sizeof(list_node) + element_size`
- `free_element` 回调函数用于释放复杂类型的元素
- 与本卡片双链表的区别：工程实现更通用，支持任意类型

---

## 对照二：头插法 O(1) 实现

**源文件**：`engineering/src/algo-prod/list/list.c`

```c
/* 头部插入 O(1) */
int list_push_front(list_t *list, const void *element)
{
    list_node_t *node;

    node = list_node_create(list->element_size, element);
    if (!node) {
        return -1;
    }

    node->next = list->head;
    if (list->head) {
        list->head->prev = node;
    } else {
        list->tail = node;  /* 空链表时更新 tail */
    }
    list->head = node;
    list->size += 1u;
    return 0;
}
```

**对照学习点**：
- 与本卡片 `dlist_prepend` 完全相同的逻辑
- 关键点：当链表为空时 (`list->head == NULL`)，需要同时更新 `tail`
- 这是链表实现中最容易出错的边界情况

---

## 对照三：尾插法 O(1) 实现

**源文件**：`engineering/src/algo-prod/list/list.c`

```c
/* 尾部插入 O(1) - 双链表优势 */
int list_push_back(list_t *list, const void *element)
{
    list_node_t *node;

    node = list_node_create(list->element_size, element);
    if (!node) {
        return -1;
    }

    node->prev = list->tail;
    if (list->tail) {
        list->tail->next = node;
    } else {
        list->head = node;  /* 空链表时更新 head */
    }
    list->tail = node;
    list->size += 1u;
    return 0;
}
```

**对照学习点**：
- 单链表无法 O(1) 尾部插入（需要遍历或维护尾指针）
- 双链表的 `tail` 指针使尾部操作达到 O(1)
- 与头插法对称：更新 `prev`/`next` 指针，维护 `head`/`tail`

---

## 对照四：头部删除 O(1) 实现

**源文件**：`engineering/src/algo-prod/list/list.c`

```c
/* 头部删除 O(1) - 支持返回被删除元素 */
int list_pop_front(list_t *list, void *out)
{
    list_node_t *node;

    if (!list || !list->head) {
        return -1;
    }

    node = list->head;
    list->head = node->next;
    if (list->head) {
        list->head->prev = NULL;
    } else {
        list->tail = NULL;  /* 删除最后一个节点时更新 tail */
    }
    list->size -= 1u;
    list_node_destroy(list, node, out != NULL, out);
    return 0;
}
```

**对照学习点**：
- 删除后检查 `list->head` 是否为 NULL，如果是则链表为空，更新 `tail = NULL`
- `out != NULL` 参数支持"偷看"功能：删除元素但不释放
- 与本卡片 `dlist_delete_head` 对应

---

## 对照五：链表清空实现

**源文件**：`engineering/src/algo-prod/list/list.c`

```c
/* 清空链表 - 遍历释放所有节点 */
void list_clear(list_t *list)
{
    list_node_t *node;
    list_node_t *next;

    if (!list) {
        return;
    }

    node = list->head;
    while (node) {
        next = node->next;
        list_node_destroy(list, node, 0, NULL);
        node = next;
    }

    list->head = NULL;
    list->tail = NULL;
    list->size = 0u;
}
```

**对照学习点**：
- 使用 `next` 保存下一个节点，避免释放后无法访问
- 这是链表遍历的标准模式：先保存 `next`，再处理 `curr`
- 与本卡片的 `slist_destroy` 类似

---

## 工程代码模式总结

| 操作 | 单链表 | 双链表 | 工程实现 |
|------|--------|--------|---------|
| 头部插入 | O(1) | O(1) | `list_push_front()` |
| 尾部插入 | O(n) | O(1) | `list_push_back()` |
| 头部删除 | O(1) | O(1) | `list_pop_front()` |
| 尾部删除 | O(n) | O(1) | `list_pop_back()` |
| 按值查找 | O(n) | O(n) | 需要额外实现 |
| 遍历 | O(n) | O(n) | `list_node_next()` |

---

## 延伸阅读

- `engineering/src/algo-prod/list/list.c` - 通用双向链表完整实现
- `engineering/src/algo-prod/list/list.h` - 链表头文件定义
- `engineering/src/redis/redis_adlist.c` - Redis 链表实现（另一种风格）
