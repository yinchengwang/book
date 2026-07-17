/**
 * @file main.c
 * @brief 线段树演示：区间查询/更新 + 懒标记 + 动态开点
 *
 * 功能覆盖：区间求和/最大值、单点更新、懒标记区间更新、动态开点
 * 编译: gcc -std=c11 -Wall -O2 -o segment_tree main.c && ./segment_tree
 */
#include <stdio.h>
#include <stdlib.h>
typedef long long ll;
typedef enum { OP_NONE, OP_ASSIGN, OP_ADD } OpType;
typedef struct { OpType type; ll val; int pending; } LazyTag;
typedef struct SegNode { int l, r; ll sum, max; LazyTag tag; struct SegNode *left, *right; } SegNode;
SegNode *root; int n;

static SegNode *node_new(int l, int r) {
    SegNode *n = calloc(1, sizeof(SegNode));
    n->l = l; n->r = r; n->max = -0x3f3f3f3f; return n;
}
static void tree_free(SegNode *n) { if (!n) return; tree_free(n->left); tree_free(n->right); free(n); }
static void pull_up(SegNode *n) {
    n->sum = (n->left ? n->left->sum : 0) + (n->right ? n->right->sum : 0);
    n->max = n->left ? n->left->max : -0x3f3f3f3f;
    if (n->right && n->right->max > n->max) n->max = n->right->max;
}
static void push_down(SegNode *n) {
    if (!n->tag.pending || !n->left || !n->right) return;
    LazyTag t = n->tag;
    int len_l = n->left->r - n->left->l + 1, len_r = n->right->r - n->right->l + 1;
    if (t.type == OP_ASSIGN) {
        n->left->sum = n->right->sum = t.val;
        n->left->max = n->right->max = t.val;
        n->left->tag = n->right->tag = (LazyTag){OP_ASSIGN, t.val, 1};
    } else {
        n->left->sum += t.val * len_l;  n->right->sum += t.val * len_r;
        n->left->max += t.val;          n->right->max += t.val;
        n->left->tag.val += t.val;      n->right->tag.val += t.val;
        n->left->tag.pending = n->right->tag.pending = 1;
    }
    n->tag.pending = 0;
}
static void range_update(SegNode *n, int L, int R, ll val, OpType op) {
    if (!n || R < n->l || n->r < L) return;
    if (L <= n->l && n->r <= R) {
        if (op == OP_ASSIGN) { n->sum = val * (n->r - n->l + 1); n->max = val; n->tag = (LazyTag){OP_ASSIGN, val, 1}; }
        else { n->sum += val * (n->r - n->l + 1); n->max += val; n->tag.val += val; n->tag.type = OP_ADD; n->tag.pending = 1; }
        return;
    }
    push_down(n);
    int mid = (n->l + n->r) >> 1;
    if (!n->left)  n->left  = node_new(n->l, mid);
    if (!n->right) n->right = node_new(mid + 1, n->r);
    range_update(n->left,  L, R, val, op); range_update(n->right, L, R, val, op); pull_up(n);
}
static ll query_sum(SegNode *n, int L, int R) {
    if (!n || R < n->l || n->r < L) return 0;
    if (L <= n->l && n->r <= R) return n->sum;
    push_down(n); return query_sum(n->left, L, R) + query_sum(n->right, L, R);
}
static ll query_max(SegNode *n, int L, int R) {
    if (!n || R < n->l || n->r < L) return -0x3f3f3f3f;
    if (L <= n->l && n->r <= R) return n->max;
    push_down(n); ll a = query_max(n->left, L, R), b = query_max(n->right, L, R); return a > b ? a : b;
}
static void point_set(SegNode *n, int pos, ll val) {
    if (!n || pos < n->l || n->r < pos) return;
    if (n->l == n->r) { n->sum = n->max = val; return; }
    push_down(n);
    int mid = (n->l + n->r) >> 1;
    if (!n->left)  n->left  = node_new(n->l, mid);
    if (!n->right) n->right = node_new(mid + 1, n->r);
    if (pos <= mid) point_set(n->left, pos, val); else point_set(n->right, pos, val); pull_up(n);
}
static void build(SegNode *n, const ll *arr) {
    if (n->l == n->r) { if (n->l < n) n->sum = n->max = arr[n->l]; return; }
    int mid = (n->l + n->r) >> 1;
    n->left = node_new(n->l, mid); n->right = node_new(mid + 1, n->r);
    build(n->left, arr); build(n->right, arr); pull_up(n);
}
static void dump(SegNode *n, ll *out) { if (!n) return; if (n->l == n->r) { out[n->l] = n->max; return; } dump(n->left, out); dump(n->right, out); }

int main(void) {
    ll arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    n = 10; root = node_new(0, n - 1); build(root, arr);
    ll snap[10];

    printf("原始数组: [1,2,3,4,5,6,7,8,9,10]\n\n");

    /* 演示1：区间求和 */
    printf("--- 区间求和 ---\n");
    printf("sum[0,4] = %lld (期望 15)\n", query_sum(root, 0, 4));
    printf("sum[2,7] = %lld (期望 33)\n", query_sum(root, 2, 7));
    printf("sum[0,9] = %lld (期望 55)\n", query_sum(root, 0, 9));

    /* 演示2：区间最大值 */
    printf("\n--- 区间最大值 ---\n");
    printf("max[0,4] = %lld (期望 5)\n", query_max(root, 0, 4));
    printf("max[3,9] = %lld (期望 10)\n", query_max(root, 3, 9));

    /* 演示3：单点更新 */
    printf("\n--- 单点更新: pos=4 -> 100 ---\n");
    point_set(root, 4, 100);
    printf("sum[0,4] = %lld (期望 110)\n", query_sum(root, 0, 4));

    /* 演示4：区间加法（懒标记） */
    printf("\n--- 区间加法: [2,7] += 10 ---\n");
    range_update(root, 2, 7, 10, OP_ADD);
    dump(root, snap);
    printf("sum[0,9] = %lld (期望 115)\n", query_sum(root, 0, 9));
    printf("数组: [%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld]\n",
           snap[0],snap[1],snap[2],snap[3],snap[4],snap[5],snap[6],snap[7],snap[8],snap[9]);

    /* 演示5：区间赋值（懒标记） */
    printf("\n--- 区间赋值: [0,4] = 5 ---\n");
    range_update(root, 0, 4, 5, OP_ASSIGN);
    dump(root, snap);
    printf("sum[0,4] = %lld (期望 25)  max[0,4] = %lld (期望 5)\n", query_sum(root, 0, 4), query_max(root, 0, 4));

    /* 演示6：嵌套操作 */
    printf("\n--- 嵌套: [3,8] += 2 后 [5,8] = 99 ---\n");
    range_update(root, 3, 8, 2, OP_ADD);
    range_update(root, 5, 8, 99, OP_ASSIGN);
    dump(root, snap);
    printf("数组: [%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld]\n",
           snap[0],snap[1],snap[2],snap[3],snap[4],snap[5],snap[6],snap[7],snap[8],snap[9]);

    tree_free(root); return 0;
}
