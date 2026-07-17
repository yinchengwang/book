/* tree scaffold — 二叉树遍历/BST/二叉堆 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════
 * Part 1: 二叉树 + 四种遍历
 * ══════════════════════════════════════════════════════════════ */
typedef struct TNode {
    int data;
    struct TNode *left, *right;
} TNode;

static TNode *tnode_new(int v) {
    TNode *n = (TNode*)malloc(sizeof(TNode));
    n->data = v; n->left = n->right = NULL;
    return n;
}

/* 手工构造一棵树:
 *        1
 *      /   \
 *     2     3
 *    / \   / \
 *   4   5 6   7
 */
static TNode *build_demo_tree(void) {
    TNode *r = tnode_new(1);
    r->left  = tnode_new(2);
    r->right = tnode_new(3);
    r->left->left  = tnode_new(4);
    r->left->right = tnode_new(5);
    r->right->left  = tnode_new(6);
    r->right->right = tnode_new(7);
    return r;
}

static void preorder(TNode *n) {
    if (!n) return;
    printf("%d ", n->data);
    preorder(n->left);
    preorder(n->right);
}

static void inorder(TNode *n) {
    if (!n) return;
    inorder(n->left);
    printf("%d ", n->data);
    inorder(n->right);
}

static void postorder(TNode *n) {
    if (!n) return;
    postorder(n->left);
    postorder(n->right);
    printf("%d ", n->data);
}

/* 层序遍历 — 用数组队列（复用 stack_queue 思路） */
static void levelorder(TNode *root) {
    if (!root) return;
    TNode *q[64];
    int head = 0, tail = 0;
    q[tail++] = root;
    while (head < tail) {
        TNode *n = q[head++];
        printf("%d ", n->data);
        if (n->left)  q[tail++] = n->left;
        if (n->right) q[tail++] = n->right;
    }
}

static void tree_free(TNode *n) {
    if (!n) return;
    tree_free(n->left);
    tree_free(n->right);
    free(n);
}

/* ══════════════════════════════════════════════════════════════
 * Part 2: 二叉搜索树 (BST)
 * ══════════════════════════════════════════════════════════════ */
static TNode *bst_insert(TNode *root, int v) {
    if (!root) return tnode_new(v);
    if (v < root->data)
        root->left = bst_insert(root->left, v);
    else if (v > root->data)
        root->right = bst_insert(root->right, v);
    return root;
}

static TNode *bst_search(TNode *root, int v) {
    if (!root || root->data == v) return root;
    return v < root->data ? bst_search(root->left, v)
                          : bst_search(root->right, v);
}

static TNode *bst_min(TNode *root) {
    while (root && root->left) root = root->left;
    return root;
}

static TNode *bst_delete(TNode *root, int v) {
    if (!root) return NULL;
    if (v < root->data) {
        root->left = bst_delete(root->left, v);
    } else if (v > root->data) {
        root->right = bst_delete(root->right, v);
    } else {
        /* 找到目标节点 */
        if (!root->left) { TNode *r = root->right; free(root); return r; }
        if (!root->right) { TNode *l = root->left; free(root); return l; }
        /* 两个子节点：用右子树最小节点替代 */
        TNode *succ = bst_min(root->right);
        root->data = succ->data;
        root->right = bst_delete(root->right, succ->data);
    }
    return root;
}

static void bst_inorder(TNode *n) {
    if (!n) return;
    bst_inorder(n->left);
    printf("%d ", n->data);
    bst_inorder(n->right);
}

/* ══════════════════════════════════════════════════════════════
 * Part 3: 二叉堆（最小堆）
 * ══════════════════════════════════════════════════════════════ */
#define HEAP_CAP 16
typedef struct {
    int data[HEAP_CAP];
    int size;
} MinHeap;

static void heap_init(MinHeap *h) { h->size = 0; }

static void heap_sift_up(MinHeap *h, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (h->data[idx] >= h->data[parent]) break;
        int t = h->data[idx];
        h->data[idx] = h->data[parent];
        h->data[parent] = t;
        idx = parent;
    }
}

static bool heap_push(MinHeap *h, int v) {
    if (h->size >= HEAP_CAP) return false;
    h->data[h->size] = v;
    heap_sift_up(h, h->size);
    h->size++;
    return true;
}

static void heap_sift_down(MinHeap *h, int idx) {
    while (1) {
        int smallest = idx;
        int left  = 2 * idx + 1;
        int right = 2 * idx + 2;
        if (left  < h->size && h->data[left]  < h->data[smallest]) smallest = left;
        if (right < h->size && h->data[right] < h->data[smallest]) smallest = right;
        if (smallest == idx) break;
        int t = h->data[idx];
        h->data[idx] = h->data[smallest];
        h->data[smallest] = t;
        idx = smallest;
    }
}

static bool heap_pop(MinHeap *h, int *out) {
    if (h->size == 0) return false;
    *out = h->data[0];
    h->data[0] = h->data[--h->size];
    if (h->size > 0) heap_sift_down(h, 0);
    return true;
}

static void heap_print(const MinHeap *h) {
    printf("[heap] size=%d | ", h->size);
    for (int i = 0; i < h->size; i++) printf("%d ", h->data[i]);
    printf("\n");
}

int main(void) {
    /* ── 二叉树遍历 ── */
    printf("=== 二叉树遍历 ===\n");
    TNode *tree = build_demo_tree();
    printf("[pre]  ");  preorder(tree);  printf("\n");
    printf("[in]   ");  inorder(tree);   printf("\n");
    printf("[post] ");  postorder(tree); printf("\n");
    printf("[level]");  levelorder(tree); printf("\n");
    tree_free(tree);

    /* ── BST ── */
    printf("\n=== 二叉搜索树 ===\n");
    TNode *bst = NULL;
    int bvals[] = {5, 3, 7, 2, 4, 6, 8};
    for (int i = 0; i < 7; i++) bst = bst_insert(bst, bvals[i]);
    printf("[bst] 中序: "); bst_inorder(bst); printf("(应升序)\n");
    printf("[bst] search(4): %s\n", bst_search(bst, 4) ? "yes" : "no");
    printf("[bst] search(9): %s\n", bst_search(bst, 9) ? "yes" : "no");
    printf("[bst] delete(3)\n");
    bst = bst_delete(bst, 3);
    printf("[bst] 中序: "); bst_inorder(bst); printf("(3已删除)\n");
    tree_free(bst);

    /* ── 二叉堆 ── */
    printf("\n=== 二叉堆（最小堆）===\n");
    MinHeap heap; heap_init(&heap);
    int hvals[] = {5, 3, 8, 1, 2, 7};
    for (int i = 0; i < 6; i++) {
        printf("[heap] push(%d) → ", hvals[i]);
        heap_push(&heap, hvals[i]);
        heap_print(&heap);
    }
    printf("[heap] pop 序列: ");
    int v;
    while (heap_pop(&heap, &v)) printf("%d ", v);
    printf("(应升序)\n");

    printf("\n=== PASS ===\n");
    return 0;
}
