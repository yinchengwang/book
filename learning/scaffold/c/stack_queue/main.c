/* stack_queue scaffold — 数组栈/链式栈/循环队列/双端队列 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* ── 1. 数组栈 ── */
#define ASTACK_CAP 8
typedef struct {
    int data[ASTACK_CAP];
    int top;                    /* 栈顶索引，-1 = 空 */
} AStack;

static void astack_init(AStack *s) { s->top = -1; }
static bool astack_push(AStack *s, int v) {
    if (s->top >= ASTACK_CAP - 1) return false;
    s->data[++s->top] = v;
    return true;
}
static bool astack_pop(AStack *s, int *out) {
    if (s->top < 0) return false;
    *out = s->data[s->top--];
    return true;
}
static void astack_print(const AStack *s) {
    printf("[astack] top=%d | ", s->top);
    for (int i = 0; i <= s->top; i++)
        printf("%d ", s->data[i]);
    printf("\n");
}

/* ── 2. 链式栈 ── */
typedef struct LNode {
    int data;
    struct LNode *next;
} LNode;

static LNode *lstack_push(LNode *top, int v) {
    LNode *n = (LNode*)malloc(sizeof(LNode));
    n->data = v;
    n->next = top;
    return n;
}
static LNode *lstack_pop(LNode *top, int *out) {
    if (!top) return NULL;
    *out = top->data;
    LNode *next = top->next;
    free(top);
    return next;
}
static void lstack_print(const LNode *top) {
    printf("[lstack] ");
    for (const LNode *p = top; p; p = p->next)
        printf("%d -> ", p->data);
    printf("NULL\n");
}

/* ── 3. 循环队列 ── */
#define CQUEUE_CAP 5
typedef struct {
    int data[CQUEUE_CAP];
    int head, tail, size;
} CQueue;

static void cq_init(CQueue *q) { q->head = 0; q->tail = 0; q->size = 0; }
static bool cq_enqueue(CQueue *q, int v) {
    if (q->size >= CQUEUE_CAP) return false;
    q->data[q->tail] = v;
    q->tail = (q->tail + 1) % CQUEUE_CAP;
    q->size++;
    return true;
}
static bool cq_dequeue(CQueue *q, int *out) {
    if (q->size == 0) return false;
    *out = q->data[q->head];
    q->head = (q->head + 1) % CQUEUE_CAP;
    q->size--;
    return true;
}
static void cq_print(const CQueue *q) {
    printf("[cqueue] size=%d head=%d tail=%d | ", q->size, q->head, q->tail);
    for (int i = 0; i < q->size; i++)
        printf("%d ", q->data[(q->head + i) % CQUEUE_CAP]);
    printf("\n");
}

/* ── 4. 双端队列（数组）── */
#define DEQUE_CAP 6
typedef struct {
    int data[DEQUE_CAP];
    int front, rear, size;
} Deque;

static void dq_init(Deque *d) { d->front = -1; d->rear = 0; d->size = 0; }
static bool dq_push_front(Deque *d, int v) {
    if (d->size >= DEQUE_CAP) return false;
    d->front = (d->front + 1) % DEQUE_CAP;
    d->data[d->front] = v;
    if (d->size == 0) d->rear = d->front;
    d->size++;
    return true;
}
static bool dq_push_back(Deque *d, int v) {
    if (d->size >= DEQUE_CAP) return false;
    d->rear = (d->rear - 1 + DEQUE_CAP) % DEQUE_CAP;
    d->data[d->rear] = v;
    if (d->size == 0) d->front = d->rear;
    d->size++;
    return true;
}
static void dq_print(const Deque *d) {
    printf("[deque] size=%d | ", d->size);
    for (int i = 0; i < d->size; i++)
        printf("%d ", d->data[(d->front - i + DEQUE_CAP) % DEQUE_CAP]);
    printf("\n");
}

int main(void) {
    int val;

    /* 数组栈 */
    printf("=== 数组栈 ===\n");
    AStack as; astack_init(&as);
    astack_push(&as, 10); astack_print(&as);
    astack_push(&as, 20); astack_print(&as);
    astack_push(&as, 30); astack_print(&as);
    astack_pop(&as, &val);
    printf("[astack] pop = %d\n", val);
    astack_print(&as);                    /* 10 20 */

    /* 链式栈 */
    printf("\n=== 链式栈 ===\n");
    LNode *ls = NULL;
    ls = lstack_push(ls, 100); lstack_print(ls);
    ls = lstack_push(ls, 200); lstack_print(ls);
    ls = lstack_pop(ls, &val);
    printf("[lstack] pop = %d\n", val);
    lstack_print(ls);                     /* 100 */

    /* 循环队列 */
    printf("\n=== 循环队列 ===\n");
    CQueue cq; cq_init(&cq);
    cq_enqueue(&cq, 1); cq_print(&cq);
    cq_enqueue(&cq, 2); cq_print(&cq);
    cq_enqueue(&cq, 3); cq_print(&cq);
    cq_dequeue(&cq, &val);
    printf("[cqueue] dequeue = %d\n", val);
    cq_print(&cq);                        /* 2 3 */
    cq_enqueue(&cq, 4); cq_enqueue(&cq, 5); cq_enqueue(&cq, 6);
    cq_print(&cq);                        /* 2 3 4 5 6 (满) */

    /* 双端队列 */
    printf("\n=== 双端队列 ===\n");
    Deque dq; dq_init(&dq);
    dq_push_back(&dq, 5);  dq_print(&dq);
    dq_push_front(&dq, 3); dq_print(&dq);
    dq_push_back(&dq, 7);  dq_print(&dq);
    dq_push_front(&dq, 1); dq_print(&dq); /* 1 3 5 7 */

    /* clean */
    while (ls) ls = lstack_pop(ls, &val);

    printf("\n=== PASS ===\n");
    return 0;
}
