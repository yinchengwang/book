/**
 * @file linked_lists/main.c
 * @brief 链表高频算法题演示
 *
 * 包含:
 * 1. 反转链表（迭代 + 递归）
 * 2. 环检测（Floyd 判环算法）
 * 3. 合并两个有序链表
 * 4. 快慢指针找中点
 */

#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// 链表节点定义
// ============================================================================

typedef struct ListNode {
    int val;
    struct ListNode *next;
} ListNode;

// ============================================================================
// 1. 反转链表
// ============================================================================

/**
 * 反转链表 - 迭代法
 * 时间: O(n), 空间: O(1)
 */
ListNode *reverse_iterative(ListNode *head) {
    ListNode *prev = NULL;
    ListNode *curr = head;
    while (curr) {
        ListNode *next = curr->next;  // 保存后继
        curr->next = prev;            // 反转指针
        prev = curr;                  // prev 前移
        curr = next;                  // curr 前移
    }
    return prev;
}

/**
 * 反转链表 - 递归法
 * 时间: O(n), 空间: O(n)（递归栈）
 */
ListNode *reverse_recursive(ListNode *head) {
    // 递归终止：空链表或只剩一个节点
    if (!head || !head->next) return head;

    ListNode *new_head = reverse_recursive(head->next);
    // 把 head 放到反转后的链表末尾
    head->next->next = head;
    head->next = NULL;
    return new_head;
}

// ============================================================================
// 2. 环检测 - Floyd 判环算法
// ============================================================================

/**
 * 检测链表是否有环（Floyd 算法）
 * 快慢指针相遇则有环
 * 时间: O(n), 空间: O(1)
 *
 * @param head 链表头
 * @return 1 有环, 0 无环
 */
int has_cycle(ListNode *head) {
    if (!head) return 0;

    ListNode *slow = head;
    ListNode *fast = head;

    while (fast && fast->next) {
        slow = slow->next;       // 慢指针走一步
        fast = fast->next->next; // 快指针走两步

        if (slow == fast) {      // 相遇即有环
            return 1;
        }
    }
    return 0;
}

// ============================================================================
// 3. 合并两个有序链表
// ============================================================================

/**
 * 合并两个升序链表
 * 时间: O(n+m), 空间: O(1)
 *
 * @param l1 有序链表 1
 * @param l2 有序链表 2
 * @return 合并后的有序链表
 */
ListNode *merge_two_lists(ListNode *l1, ListNode *l2) {
    // 哑节点简化边界处理
    ListNode dummy = {0, NULL};
    ListNode *tail = &dummy;

    while (l1 && l2) {
        if (l1->val <= l2->val) {
            tail->next = l1;
            l1 = l1->next;
        } else {
            tail->next = l2;
            l2 = l2->next;
        }
        tail = tail->next;
    }

    // 拼接剩余部分
    tail->next = l1 ? l1 : l2;
    return dummy.next;
}

// ============================================================================
// 4. 快慢指针找中点
// ============================================================================

/**
 * 返回链表的中间节点
 * 偶数个节点返回第二个中间节点
 * 时间: O(n), 空间: O(1)
 */
ListNode *middle_node(ListNode *head) {
    ListNode *slow = head;
    ListNode *fast = head;

    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }
    return slow;
}

// ============================================================================
// 辅助函数
// ============================================================================

/** 根据数组创建链表 */
ListNode *list_create(int *arr, int size) {
    ListNode dummy = {0, NULL};
    ListNode *tail = &dummy;
    for (int i = 0; i < size; i++) {
        ListNode *node = (ListNode *)malloc(sizeof(ListNode));
        node->val = arr[i];
        node->next = NULL;
        tail->next = node;
        tail = node;
    }
    return dummy.next;
}

/** 打印链表 */
void list_print(ListNode *head) {
    printf("[");
    while (head) {
        printf("%d", head->val);
        if (head->next) printf(" -> ");
        head = head->next;
    }
    printf("]\n");
}

/** 释放链表 */
void list_free(ListNode *head) {
    while (head) {
        ListNode *tmp = head;
        head = head->next;
        free(tmp);
    }
}

// ============================================================================
// 主函数
// ============================================================================

int main(void) {
    printf("=== 链表高频算法题演示 ===\n\n");

    // ------------------------------------------------------------
    // 1. 反转链表测试
    // ------------------------------------------------------------
    printf("--- 1. 反转链表 ---\n");
    int arr1[] = {1, 2, 3, 4, 5};
    ListNode *list1 = list_create(arr1, 5);
    printf("原链表:   ");
    list_print(list1);

    ListNode *rev1 = reverse_iterative(list1);
    printf("迭代反转: ");
    list_print(rev1);

    ListNode *rev2 = reverse_recursive(rev1);
    printf("递归反转: ");
    list_print(rev2);
    list_free(rev2);

    // ------------------------------------------------------------
    // 2. 环检测测试
    // ------------------------------------------------------------
    printf("\n--- 2. 环检测 (Floyd 算法) ---\n");
    ListNode *list2 = list_create(arr1, 5);
    printf("无环链表: has_cycle = %d\n", has_cycle(list2));

    // 制造环: 尾节点指向第 2 个节点
    ListNode *tail = list2;
    while (tail->next) tail = tail->next;
    tail->next = list2->next;  // 指向值 2 的节点
    printf("有环链表: has_cycle = %d\n", has_cycle(list2));
    // 断开环再释放 (避免 free 死循环)
    tail->next = NULL;
    list_free(list2);

    // ------------------------------------------------------------
    // 3. 合并有序链表测试
    // ------------------------------------------------------------
    printf("\n--- 3. 合并两个有序链表 ---\n");
    int arr3a[] = {1, 3, 5, 7};
    int arr3b[] = {2, 4, 6, 8};
    ListNode *l1 = list_create(arr3a, 4);
    ListNode *l2 = list_create(arr3b, 4);
    printf("链表 1: "); list_print(l1);
    printf("链表 2: "); list_print(l2);

    ListNode *merged = merge_two_lists(l1, l2);
    printf("合并后:  "); list_print(merged);
    list_free(merged);

    // ------------------------------------------------------------
    // 4. 快慢指针找中点测试
    // ------------------------------------------------------------
    printf("\n--- 4. 快慢指针找中点 ---\n");
    int arr4a[] = {1, 2, 3, 4, 5};
    ListNode *list4a = list_create(arr4a, 5);
    printf("奇数个节点: "); list_print(list4a);
    printf("中点值: %d\n", middle_node(list4a)->val);
    list_free(list4a);

    int arr4b[] = {1, 2, 3, 4};
    ListNode *list4b = list_create(arr4b, 4);
    printf("偶数个节点: "); list_print(list4b);
    printf("中点值: %d\n", middle_node(list4b)->val);
    list_free(list4b);

    printf("\n=== 演示完成 ===\n");
    return 0;
}
