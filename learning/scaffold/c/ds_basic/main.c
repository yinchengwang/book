/* ds_basic scaffold — 单链表/双向链表/哨兵节点/循环检测 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* ── 单链表 ── */
typedef struct SNode {
    int data;
    struct SNode *next;
} SNode;

static SNode *slist_insert_head(SNode *head, int val) {
    SNode *n = (SNode*)malloc(sizeof(SNode));
    n->data = val;
    n->next = head;
    return n;
}

static void slist_print(const SNode *head) {
    printf("[slist] ");
    for (const SNode *p = head; p; p = p->next)
        printf("%d -> ", p->data);
    printf("NULL\n");
}

static SNode *slist_find(SNode *head, int val) {
    for (SNode *p = head; p; p = p->next)
        if (p->data == val) return p;
    return NULL;
}

static SNode *slist_delete(SNode *head, int val) {
    SNode dummy = {0, head};
    SNode *prev = &dummy, *cur = head;
    while (cur) {
        if (cur->data == val) {
            prev->next = cur->next;
            free(cur);
            break;
        }
        prev = cur;
        cur = cur->next;
    }
    return dummy.next;
}

static void slist_free(SNode *head) {
    while (head) {
        SNode *tmp = head;
        head = head->next;
        free(tmp);
    }
}

/* ── 双向链表 ── */
typedef struct DNode {
    int data;
    struct DNode *prev;
    struct DNode *next;
} DNode;

static DNode *dlist_append(DNode *head, int val) {
    DNode *n = (DNode*)malloc(sizeof(DNode));
    n->data = val;
    n->next = NULL;
    if (!head) { n->prev = NULL; return n; }
    DNode *tail = head;
    while (tail->next) tail = tail->next;
    tail->next = n;
    n->prev = tail;
    return head;
}

static void dlist_print(const DNode *head) {
    printf("[dlist] ");
    for (const DNode *p = head; p; p = p->next)
        printf("%d <-> ", p->data);
    printf("NULL\n");
}

/* 反向遍历（验证 prev 指针） */
static void dlist_print_rev(const DNode *head) {
    if (!head) return;
    const DNode *tail = head;
    while (tail->next) tail = tail->next;
    printf("[dlist-rev] ");
    for (const DNode *p = tail; p; p = p->prev)
        printf("%d <-> ", p->data);
    printf("NULL\n");
}

static void dlist_free(DNode *head) {
    while (head) {
        DNode *tmp = head;
        head = head->next;
        free(tmp);
    }
}

/* ── 循环检测（Floyd 判圈算法）── */
static bool has_cycle(SNode *head) {
    if (!head) return false;
    SNode *slow = head, *fast = head;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) return true;
    }
    return false;
}

int main(void) {
    /* 1. 单链表 CRUD */
    printf("=== 单链表 CRUD ===\n");
    SNode *s = NULL;
    s = slist_insert_head(s, 30);
    s = slist_insert_head(s, 20);
    s = slist_insert_head(s, 10);
    slist_print(s);                        /* 10 20 30 */

    SNode *found = slist_find(s, 20);
    printf("[slist] find(20) = %s\n", found ? "yes" : "no");

    printf("[slist] delete(20)\n");
    s = slist_delete(s, 20);
    slist_print(s);                        /* 10 30 */

    /* 2. 双向链表 */
    printf("\n=== 双向链表 ===\n");
    DNode *d = NULL;
    d = dlist_append(d, 1);
    d = dlist_append(d, 2);
    d = dlist_append(d, 3);
    dlist_print(d);                        /* 1 2 3 */
    dlist_print_rev(d);                    /* 3 2 1 */

    /* 3. 哨兵节点 — 简化边界条件 */
    printf("\n=== 哨兵节点 ===\n");
    SNode sentinel = {0, NULL};            /* 栈上哨兵，不存数据 */
    SNode *a = (SNode*)malloc(sizeof(SNode));
    SNode *b = (SNode*)malloc(sizeof(SNode));
    a->data = 100; a->next = b;
    b->data = 200; b->next = NULL;
    sentinel.next = a;
    printf("[sentinel] 遍历: ");
    for (SNode *p = sentinel.next; p; p = p->next)
        printf("%d ", p->data);
    printf("\n");
    free(a); free(b);

    /* 4. 循环检测 */
    printf("\n=== 循环检测 (Floyd) ===\n");
    SNode *c1 = (SNode*)malloc(sizeof(SNode));
    SNode *c2 = (SNode*)malloc(sizeof(SNode));
    SNode *c3 = (SNode*)malloc(sizeof(SNode));
    c1->data = 1; c1->next = c2;
    c2->data = 2; c2->next = c3;
    c3->data = 3; c3->next = c2;           /* 制造环: 3→2 */
    printf("[cycle] has_cycle(带环链表) = %s\n", has_cycle(c1) ? "yes" : "no");
    slist_free(s);  /* 带环链表不能 free，跳过 c1/c2/c3 */
    dlist_free(d);
    printf("[cycle] 带环链表未 free（避免 double-free）\n");

    printf("\n=== PASS ===\n");
    return 0;
}
