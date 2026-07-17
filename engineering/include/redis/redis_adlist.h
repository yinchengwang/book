/* redis's A generic doubly linked list implementation */

#ifndef REDIS_AD_LIST_H
#define REDIS_AD_LIST_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct redis_list_node {
    struct redis_list_node *prev;
    struct redis_list_node *next;
    void *val;
} redis_dlist_node_t;

typedef enum redis_list_iter_dir {
    AL_START_HEAD = 0,
    AL_START_TAIL,
} redis_list_iter_dir_e;

typedef struct redis_list_iter {
    // 遍历时的下一个结点
    redis_dlist_node_t *next;
    // 遍历时的方向
    redis_list_iter_dir_e direction;
} redis_list_iter_t;

typedef struct redis_dlist {
    // 表头节点
    redis_dlist_node_t *head;
    // 表尾节点
    redis_dlist_node_t *tail;
    // 节点复制函数
    void *(*dup)(void *ptr);
    // 节点释放函数
    void (*free)(void *ptr);
    // 节点值对比函数
    int (*match)(void *ptr, void *key);
    // 链表节点数量
    unsigned long len;
} redis_dlist_t;

/* (l)的作用:
 * 简单指针: LIST_LEN(ptr) -> ptr->len -> ok
 * 表达式: LIST_LEN(ptr + 1) -> ptr + 1->len -> error
 * 三目运算: LIST_LEN(cond ? p1 : p2) -> cond ? p1 : p2->len -> error
 */

/* list macros */
#define REDIS_LIST_LEN(l) ((l)->len)

#define REDIS_LIST_FIRST(l) ((l)->head)

#define REDIS_LIST_LAST(l) ((l)->tail)

/* list node macros */
#define REDIS_LIST_PRE_NODE(n) ((n)->prev)

#define REDIS_LIST_NEXT_NODE(n) ((n)->next)

#define REDIS_LIST_NODE_VALUE(n) ((n)->val)

/* list method macros */
#define REDIS_LIST_SET_DUP_METHOD(l, m) ((l)->dup = (m))

#define REDIS_LIST_SET_FREE_METHOD(l, m) ((l)->free = (m))

#define REDIS_LIST_SET_MATCH_METHOD(l, m) ((l)->match = (m))

#define REDIS_LIST_GET_DUP_METHOD(l, m) ((l)->dup)

#define REDIS_LIST_GET_FREE_METHOD(l, m) ((l)->free)

#define REDIS_LIST_GET_MATCH_METHOD(l, m) ((l)->match)


/* function prototypes */
/* list create/release */
redis_dlist_t *redis_list_create();

void redis_list_empty(redis_dlist_t *list);

void redis_list_release(redis_dlist_t *list);

void redis_list_release_generic(void *list);

/* list link/unlink */
void redis_list_link_node_head(redis_dlist_t *rlist, redis_dlist_node_t *node);

void redis_list_link_node_tail(redis_dlist_t *rlist, redis_dlist_node_t *node);

void redis_list_unlink_node(redis_dlist_t *rlist, redis_dlist_node_t *node);

/* list insert/delete */
redis_dlist_t *redis_list_add_node_head(redis_dlist_t *rlist, void *value);

redis_dlist_t *redis_list_add_node_tail(redis_dlist_t *rlist, void *value);

redis_dlist_t *redis_list_insert_node(redis_dlist_t *rlist, redis_dlist_node_t *old_node, void *value, bool after);

void redis_list_del_node(redis_dlist_t *rlist, redis_dlist_node_t *node);

/* list iterator */
redis_list_iter_t *redis_list_get_iter(redis_dlist_t *rlist, redis_list_iter_dir_e direction);

void redis_list_release_iter(redis_list_iter_t *iter);

redis_dlist_node_t *redis_list_next(redis_list_iter_t *iter);

void redis_list_iter_rewind(redis_dlist_t *rlist, redis_list_iter_t *iter);

void redis_list_iter_rewind_tail(redis_dlist_t *rlist, redis_list_iter_t *iter);

/* list rotate */
void redis_list_rotate_head_to_tail(redis_dlist_t *rlist);

void redis_list_rotate_tail_to_head(redis_dlist_t *rlist);

/* list dup */
redis_dlist_t *redis_list_dup(redis_dlist_t *ori_rlist);

/* list search */
redis_dlist_node_t *redis_list_search_key(redis_dlist_t *rlist, void *key);

redis_dlist_node_t *redis_list_index(redis_dlist_t *rlist, long index);

/* list merge */
redis_dlist_t *redis_list_join(redis_dlist_t *l, redis_dlist_t *o);


void redis_list_init_node(redis_dlist_node_t *node, void *value);

#ifdef __cplusplus
}
#endif // extern "C"

#endif // REDIS_AD_LIST_H