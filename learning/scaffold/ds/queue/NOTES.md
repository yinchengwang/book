# 工程对照笔记

## 概述

本学习卡片演示的队列概念直接对应工程代码中的实际应用。Redis 使用链表实现列表操作，本质上就是队列的实现。以下对照工程源码中的队列使用模式。

---

## 对照一：Redis 列表作为队列

**源文件**：`engineering/src/redis/redis_adlist.c`

Redis 的双向链表是队列的经典实现，通过 `LPUSH`/`RPOP` 或 `RPUSH`/`LPOP` 实现队列操作：

```c
/* Redis 链表节点结构 */
typedef struct redis_dlist_node {
    struct redis_dlist_node *prev;
    struct redis_dlist_node *next;
    void *val;
} redis_dlist_node_t;

/* Redis 链表结构 */
typedef struct redis_dlist {
    redis_dlist_node_t *head;
    redis_dlist_node_t *tail;
    unsigned long len;
    // 回调函数指针
    void *(*dup)(void *ptr);
    void (*free)(void *ptr);
    int (*match)(void *ptr, void *key);
} redis_dlist_t;
```

**队列操作实现**：

```c
/* 入队：从尾部添加 O(1) */
redis_dlist_t *redis_list_add_node_tail(redis_dlist_t *rlist, void *value)
{
    redis_dlist_node_t *new_node = malloc(sizeof(redis_dlist_node_t));
    new_node->val = value;

    if (rlist->len == 0) {
        rlist->head = rlist->tail = new_node;
        new_node->prev = new_node->next = NULL;
    } else {
        new_node->prev = rlist->tail;
        new_node->next = NULL;
        rlist->tail->next = new_node;
        rlist->tail = new_node;
    }
    rlist->len++;
    return rlist;
}

/* 出队：从头部删除 O(1) */
void redis_list_del_node(redis_dlist_t *rlist, redis_dlist_node_t *node)
{
    redis_list_unlink_node(rlist, node);
    if (rlist->free) {
        rlist->free(node->val);
    }
    free(node);
}
```

**对照学习点**：
- Redis 链表实现本质上是链式队列
- `redis_list_add_node_tail` = `enqueue`，O(1)
- `redis_list_del_node` = `dequeue`，O(1)
- 与本卡片链式队列的区别：Redis 用通用指针 `void *val`，支持任意类型

---

## 对照二：Redis 列表迭代器

**源文件**：`engineering/src/redis/redis_adlist.c`

Redis 列表提供迭代器接口，方便遍历队列：

```c
/* 列表迭代器 */
typedef struct redis_list_iter {
    redis_dlist_node_t *next;
    redis_list_iter_dir_e direction;  /* AL_START_HEAD or AL_START_TAIL */
} redis_list_iter_t;

/* 获取迭代器 */
redis_list_iter_t *redis_list_get_iter(redis_dlist_t *rlist,
                                        redis_list_iter_dir_e direction)
{
    redis_list_iter_t *iter = malloc(sizeof(redis_list_iter_t));
    if (direction == AL_START_HEAD) {
        iter->next = rlist->head;  /* 从头遍历 = 队列顺序 */
    } else {
        iter->next = rlist->tail;  /* 从尾遍历 = 逆序 */
    }
    iter->direction = direction;
    return iter;
}

/* 迭代下一个元素 */
redis_dlist_node_t *redis_list_next(redis_list_iter_t *iter)
{
    redis_dlist_node_t *current = iter->next;
    if (current) {
        if (iter->direction == AL_START_HEAD) {
            iter->next = current->next;
        } else {
            iter->next = current->prev;
        }
    }
    return current;
}
```

**对照学习点**：
- 迭代器方向 `AL_START_HEAD` 正好是队列的 FIFO 遍历顺序
- 可用于实现"偷看队头"而不删除的操作

---

## 对照三：列表旋转操作

**源文件**：`engineering/src/redis/redis_adlist.c`

Redis 列表支持旋转操作，将队头移到队尾：

```c
/* 旋转：队头移到队尾 O(1) */
void redis_list_rotate_head_to_tail(redis_dlist_t *rlist)
{
    redis_dlist_node_t *head = rlist->head;
    head->next->prev = NULL;
    rlist->head = head->next;

    rlist->tail->next = head;
    head->next = NULL;
    head->prev = rlist->tail;
    rlist->tail = head;
}
```

**对照学习点**：
- 这是队列的变种操作，用于循环处理
- 时间复杂度 O(1)，只调整指针

---

## 工程代码模式总结

| 组件 | 实现方式 | 操作 | 时间复杂度 |
|------|---------|------|-----------|
| Redis List | 双向链表 | `add_node_tail` (enqueue) | O(1) |
| Redis List | 双向链表 | `del_node` (dequeue) | O(1) |
| Redis List | 双向链表 | `rotate` (旋转) | O(1) |
| Redis List | 双向链表 | 迭代遍历 | O(n) |

---

## 延伸阅读

- `engineering/src/redis/redis_adlist.c` - Redis 链表完整实现
- `engineering/src/redis/redis_adlist.h` - 链表头文件定义
- Redis 官方文档：List 命令 (LPUSH, RPUSH, LPOP, RPOP)
