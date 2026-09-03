#include<stdio.h>
#include<stdlib.h>
#include <stdbool.h>

struct ListNode {
    int val;
    struct ListNode *next;
};

struct TreeNode {
    int val;
    struct TreeNode *left;
    struct TreeNode *right;
};


bool dfs(struct TreeNode *node, struct ListNode *head)
{
    if (head == NULL) {
        return true;
    }

    if (node == NULL) {
        return false;
    }

    if (node->val != head->val) {
        return false;
    }

    return dfs(node->left, head->next) || dfs(node->right, head->next);
}

bool isSubPath(struct ListNode* head, struct TreeNode* root)
{
    if (root == NULL) {
        return false;
    }

    return dfs(root, head) || isSubPath(head, root->left) || isSubPath(head, root->right);
}