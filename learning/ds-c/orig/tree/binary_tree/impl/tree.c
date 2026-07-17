#include <stdio.h>
#include <stdlib.h>

#include "ds/queue.h"
#include "ds/tree.h"

static tree_node_t *create_tree_node(int element)
{
    tree_node_t *node = (tree_node_t *)malloc(sizeof(tree_node_t));
    if (!node) {
        printf("malloc tree node failed.\n");
        return NULL;
    }

    node->val = element;
    node->left = NULL;
    node->right = NULL;

    return node;
}

tree_node_t *create_tree(int *arr, int size)
{
    if (!arr) {
        printf("[create_tree] tree data arr is NULL.\n");
        return NULL;
    }

    queue_t *queue = queue_create(0, QUEUE_TYPE_LINKED_LIST);
    if (!queue) {
        printf("[create_tree] queue create failed.\n");
        return NULL;
    }

    int v_idx = 0;
    tree_node_t *root = create_tree_node(arr[v_idx++]);
    if (!root) {
        queue_destroy(queue);
        printf("[create_tree] create root node failed.\n");
        return NULL;
    }

    if (enqueue(queue, TO_VOID_PTR(root)) != STATUS_OK) {
        queue_destroy(queue);
        tree_destroy(root);
        printf("[create_tree] root node enqueue failed.\n");
        return NULL;
    }

    while (!is_queue_empty(queue) && v_idx < size) {
        void *data = front(queue);
        if (!data) {
            queue_destroy(queue);
            tree_destroy(root);
            printf("[create_tree] root node enqueue failed.\n");
            return NULL;
        }
        tree_node_t *curr = (tree_node_t *)data;

        // 插入左节点
        // 暂时使用-1表示空节点
        if (v_idx < size && arr[v_idx] != -1) {
            tree_node_t *left_node = create_tree_node(arr[v_idx]);
            if (!left_node) {
                queue_destroy(queue);
                tree_destroy(root);
                printf("[create_tree] create left node failed.\n");
                return NULL;
            }
            curr->left = left_node;

            if (enqueue(queue, TO_VOID_PTR(left_node)) != STATUS_OK) {
                queue_destroy(queue);
                tree_destroy(root);
                printf("[create_tree] left node enqueue failed.\n");
                return NULL;
            }
        }
        v_idx++;

        // 插入右节点
        if (v_idx < size && arr[v_idx] != -1) {
            tree_node_t *right_node = create_tree_node(arr[v_idx]);
            if (!right_node) {
                queue_destroy(queue);
                tree_destroy(root);
                printf("[create_tree] create right node failed.\n");
                return NULL;
            }
            curr->right = right_node;

            if (enqueue(queue, TO_VOID_PTR(right_node)) != STATUS_OK) {
                queue_destroy(queue);
                tree_destroy(root);
                printf("[create_tree] right node enqueue failed.\n");
                return NULL;
            }
        }
        v_idx++;

        if (dequeue(queue, NULL) != STATUS_OK) {
            queue_destroy(queue);
            tree_destroy(root);
            printf("[create_tree] dequeue node failed.\n");
            return NULL;
        }
    }

    return root;
}

void bst_insert(tree_node_t **root, int val)
{
    if (*root == NULL) {
        *root = create_tree_node(val);
    } else if (val < (*root)->val) {
        bst_insert(&(*root)->left, val);
    } else if (val > (*root)->val) {
        bst_insert(&(*root)->right, val);
    }
}

static void pre_order_internal(tree_node_t *root, int *ret, uint32_t *return_size)
{
    if (!root) {
        return;
    }

    ret[(*return_size)++] = root->val;
    pre_order_internal(root->left, ret, return_size);
    pre_order_internal(root->right, ret, return_size);
}

// 二叉树的前序遍历
int *pre_order(tree_node_t *root, uint32_t *return_size)
{
    *return_size = 0;
    if (!root) {
        return NULL;
    }

    int *ret = (int *)malloc(sizeof(int) * 100);
    if (!ret) {
        printf("[pre_order] malloc tree node failed.\n");
        return NULL;
    }

    pre_order_internal(root, ret, return_size);

    return ret;
}

static void in_order_internal(tree_node_t *root, int *ret, uint32_t *return_size)
{
    if (!root) {
        return;
    }

    in_order_internal(root->left, ret, return_size);
    ret[(*return_size)++] = root->val;
    in_order_internal(root->right, ret, return_size);
}

// 二叉树的中序遍历
int *in_order(tree_node_t *root, uint32_t *return_size)
{
    *return_size = 0;
    if (!root) {
        return NULL;
    }

    int *ret = (int*)malloc(sizeof(int) * 1000);
    if (!ret) {
        printf("[in_order] malloc tree node failed.\n");
        return NULL;
    }

    in_order_internal(root, ret, return_size);

    return ret;
}

static void post_order_internal(tree_node_t *root, int *ret, uint32_t *return_size)
{
    if (!root) {
        return;
    }

    post_order_internal(root->left, ret, return_size);
    post_order_internal(root->right, ret, return_size);
    ret[(*return_size)++] = root->val;
}

// 二叉树的后续遍历
int *post_order(tree_node_t *root, uint32_t *return_size)
{
    *return_size = 0;
    if (!root) {
        return NULL;
    }

    int *ret = (int*)malloc(sizeof(int) * 1000);
    if (!ret) {
        printf("[post_order] malloc tree node failed.\n");
        return NULL;
    }

    post_order_internal(root, ret, return_size);

    return ret;
}

// 二叉树的层序遍历
int *level_order(tree_node_t *root, uint32_t *return_size)
{
    *return_size = 0;
    if (!root) {
        return NULL;
    }

    int *res = (int*)malloc(sizeof(int) * 1000);
    if (!res) {
        printf("[level_order] malloc result failed.\n");
        return NULL;
    }

    queue_t *queue = queue_create(0, QUEUE_TYPE_LINKED_LIST);
    if (!queue) {
        printf("[level_order] queue_create failed.\n");
        return NULL;
    }

    int ret = enqueue(queue, TO_VOID_PTR(root));
    if (ret != STATUS_OK) {
        queue_destroy(queue);
        printf("[level_order] enqueue tree node failed.\n");
        return NULL;
    }

    while (!is_queue_empty(queue)) {
        void *data = front(queue);
        if (!data) {
            queue_destroy(queue);
            return NULL;
        }

        tree_node_t *node = (tree_node_t *)data;
        // printf("node->val = %d.\n", node->val);
        res[(*return_size)++] = node->val;

        ret = dequeue(queue, NULL);
        if (ret != STATUS_OK) {
            queue_destroy(queue);
            printf("[level_order] dequeue failed.\n");
            return NULL;
        }

        if (node->left) {
            ret = enqueue(queue, TO_VOID_PTR(node->left));
            if (ret != STATUS_OK) {
                queue_destroy(queue);
                printf("[level_order] enqueue left node failed.\n");
                return NULL;
            }
        }

        if (node->right) {
            ret = enqueue(queue, TO_VOID_PTR(node->right));
            if (ret != STATUS_OK) {
                queue_destroy(queue);
                printf("[level_order] enqueue right node failed.\n");
                return NULL;
            }
        }
    }

    return res;
}

// 树高度
int get_tree_height(tree_node_t *root)
{
    if (!root) {
        return 0;
    }

    int left_height = get_tree_height(root->left);
    int right_height = get_tree_height(root->right);

    return (left_height > right_height ? left_height : right_height) + 1;
}

void tree_destroy(tree_node_t *root)
{
    if (!root) {
        return;
    }

    tree_destroy(root->left);
    tree_destroy(root->right);

    free(root);
}

bool find_sum_path(tree_node_t *node, int target, int curr_sum)
{
    if (!node) {
        return false;
    }

    curr_sum += node->val;
    if (!node->left && !node->right && curr_sum == target) {
        return true;
    }

    bool left_result = find_sum_path(node->left, target, curr_sum);
    bool right_result = find_sum_path(node->right, target, curr_sum);

    return left_result || right_result;
}

bool tree_has_sum_path(tree_node_t *root, int sum)
{
    return find_sum_path(root, sum, 0);
}

bool is_mirror(tree_node_t *lhs, tree_node_t *rhs)
{
    if (!lhs && !rhs) {
        return true;
    }

    if (!lhs || !rhs || lhs->val != rhs->val) {
        return false;
    }

    return is_mirror(lhs->left, rhs->right) && is_mirror(lhs->right, rhs->left);
}

// 判断树是否是镜像对称
bool is_tree_symmetric(tree_node_t *root)
{
    if (!root) {
        return false;
    }

    return is_mirror(root->left, root->right);
}