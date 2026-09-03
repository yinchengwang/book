#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "ds/list.h"

list_node_t *list_node_create(int val)
{
    list_node_t *node = (list_node_t *)malloc(sizeof(list_node_t));
    if (!node) {
        printf("[list_node_create] list node create failed.\n");
        return NULL;
    }

    node->val = val;
    node->next = NULL;

    return node;
}

void list_destroy(list_node_t *head)
{
    if (!head) {
        return;
    }

    list_node_t *tmp = head;
    while (tmp) {
        list_node_t *to_delete = tmp;
        tmp = tmp->next;
        free(to_delete);
    }
}

list_node_t *list_create(int *arr, int size)
{
    if (!arr || size <= 0) {
        printf("[list_create] list create failed, size: %d.\n", size);
        return NULL;
    }

    list_node_t *head = NULL;
    list_node_t *tail = NULL;

    for (int i = 0; i < size; i++) {
        list_node_t *new_node = list_node_create(arr[i]);
        if (!new_node) {
            printf("[list_create] list new node create failed.\n");
            list_destroy(head);
            return NULL;
        }

        if (!head) {
            head = new_node;
            tail = new_node;
        } else {
            tail->next = new_node;
            tail = new_node;
        }
    }

    return head;
}

void list_insert_head(list_node_t** head, int val)
{
    list_node_t *new_node = list_node_create(val);
    if (!new_node) {
        printf("[list_insert_head] list insert head failed.\n");
        return;
    }

    new_node->next = *head;
    *head = new_node;
}

void list_insert_tail(list_node_t** head, int val)
{
    list_node_t *new_node = list_node_create(val);
    if (!new_node) {
        printf("[list_insert_tail] list insert head failed.\n");
        return;
    }

    if (*head == NULL) {
        *head = new_node;
    } else {
        list_node_t *curr = *head;
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = new_node;
    }
}

list_node_t *list_get_tail(list_node_t *head)
{
    if (!head) {
        return NULL;
    }

    list_node_t *tail = head;
    while (tail->next) {
        tail = tail->next;
    }

    return tail;
}

uint32_t list_get_length(list_node_t *head)
{
    if (!head) {
        return 0;
    }

    list_node_t *curr = head;
    uint32_t len = 0;
    while (curr) {
        len++;
        curr = curr->next;
    }

    return len;
}

list_node_t *list_get_kth_node(list_node_t *head, uint32_t k)
{
    if (!head || k == 0) {
        return head;
    }

    list_node_t *kth = head;
    while (kth->next && k > 0) {
        kth = kth->next;
        k--;
    }

    return kth;
}

// 反转整个链表
list_node_t *list_reverse(list_node_t *head)
{
    if (!head || !head->next) {
        return head;
    }

    list_node_t *cur = head;
    list_node_t *pre = NULL;
    while (cur) {
        list_node_t *next = cur->next;
        cur->next = pre;
        pre = cur;
        cur = next;
    }

    return pre;
}

// 反转指定区间的链表
list_node_t *reverse_list_between(list_node_t *head, int m, int n)
{
    if (!head || !head->next || m == n) {
        return head;
    }

    list_node_t *cur = malloc(sizeof(list_node_t));
    cur->next = head;
    list_node_t *pre = cur;
    for (int i = 0; i < m - 1; i++) {
        pre = pre->next; // 反转结点的pre结点
    }

    list_node_t *start = pre->next; // 区间开始的结点
    list_node_t *next = start->next; // 需要反转的结点
    for (int i = 0; i < n - m; i++) {
        start->next = next->next;
        next->next = pre->next;
        pre->next = next;
        next = start->next;
    }

    return cur->next;
}

// k个一组翻转链表
list_node_t *list_reverse_k_group(list_node_t *head, int k)
{
    if (!head || k <= 1) {
        return head;
    }

    list_node_t *current = head;
    for (int i = 0; i < k; i++) {
        if (current == NULL) {
            return head; // 不足k个节点，不翻转
        }
        current = current->next;
    }

    list_node_t *cur = head;
    list_node_t *pre = NULL;
    list_node_t *next = NULL;
    int count = 0;
    while (cur && count < k) {
        count++;
        next = cur->next;
        cur->next = pre;
        pre = cur;
        cur = next;
    }

    if (next) {
        head->next = list_reverse_k_group(next, k);
    }

    return pre;
}


// 链表相加
list_node_t *list_node_add(list_node_t *head1, list_node_t *head2)
{
    list_node_t *rh1 = list_reverse(head1);
    list_node_t *rh2 = list_reverse(head2);

    list_node_t *head = NULL;
    list_node_t *tail = NULL;
    int carry = 0;
    while (rh1 || rh2) {
        int n1 = rh1 != NULL ? rh1->val : 0;
        int n2 = rh2 != NULL ? rh2->val : 0;
        int sum = n1 + n2 + carry;
        if (!head) {
            head = tail = malloc(sizeof(list_node_t));
            tail->val = sum % 10;
            tail->next = NULL;
        } else {
            tail->next = malloc(sizeof(list_node_t));
            tail->next->val = sum % 10;
            tail = tail->next;
            tail->next = NULL;
        }

        carry = sum / 10;
        if (rh1) {
            rh1 = rh1->next;
        }

        if (rh2) {
            rh2 = rh2->next;
        }
    }

    if (carry > 0) {
        tail->next = malloc(sizeof(list_node_t));
        tail->next->val = carry;
        tail->next->next = NULL;
    }

    // list_node_t *res = list_reverse(head);
    return head;
}


// 判断一个链表是否为回文结构
bool is_list_pail(list_node_t *head)
{
    if (!head || !head->next) {
        return true;
    }

    list_node_t *slow = head;
    list_node_t *fast = head;
    while (fast->next && fast->next->next) {
        slow = slow->next;
        fast = fast->next->next;
    }

    list_node_t *mid = slow->next;
    list_node_t *l2 = list_reverse(mid);
    list_node_t *l1 = head;

    while (l1 && l2) {
        printf("l1: %d, l2: %d", l1->val, l2->val);
        if (l1->val != l2->val) {
            return false;
        }

        l1 = l1->next;
        l2 = l2->next;
    }

    // 恢复链表的后半部分
    list_reverse(mid);

    return true;
}


// 两个链表的第一个公共结点
list_node_t *list_first_common_node(list_node_t *pHead1, list_node_t *pHead2)
{
    list_node_t *p1 = pHead1;
    list_node_t *p2 = pHead2;

    while (p1 != p2) {
        p1 = p1 ? p1->next : pHead2;
        p2 = p2 ? p2->next : pHead1;
    }

    return p1;
}


// 链表的奇偶重排
list_node_t *odd_even_list(list_node_t *head)
{
    if (!head || !head->next) {
        return head;
    }

    list_node_t *odd = head;
    list_node_t *even_head = head->next;
    list_node_t *even = even_head;
    while (even && even->next) {
        odd->next = even->next;
        odd = odd->next;
        even->next = odd->next;
        even = even->next;
    }

    odd->next = even_head;
    return head;
}


// 将有序链表中重复的元素删到只保留一个
list_node_t *list_duplicates(list_node_t *head)
{
    if (!head) {
        return head;
    }

    list_node_t *pre = head;
    list_node_t *cur = head->next;
    while (cur) {
        if (cur->val == pre->val) {
            pre->next = cur->next;
        } else {
            pre = cur;
        }

        cur = cur->next;
    }

    return head;
}


// 将有序链表中重复的元素删除
list_node_t *list_delete_duplicates(list_node_t *head)
{
    if (!head) {
        return head;
    }

    list_node_t *dummy = malloc(sizeof(list_node_t));
    dummy->next = head;

    list_node_t *cur = dummy;
    while (cur->next && cur->next->next) {
        if (cur->next->val == cur->next->next->val) {
            int x = cur->next->val;
            while (cur->next && cur->next->val == x) {
                cur->next = cur->next->next;
            }
        } else {
            cur = cur->next;
        }
    }

    return dummy->next;
}

// 判断链表中是否有环
bool list_has_cycle(list_node_t *head)
{
    if (!head) {
        return false;
    }

    list_node_t *slow = head;
    list_node_t *fast = head;

    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) {
            return true;
        }
    }

    return false;
}

// 链表中环的入口结点
list_node_t *list_cycle_entry(list_node_t *pHead)
{
    list_node_t *slow = pHead;
    list_node_t *fast = pHead;

    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) {
            slow = pHead;
            while (slow != fast) {
                slow = slow->next;
                fast = fast->next;
            }

            return slow;
        }
    }

    return NULL;
}

// 合并有序链表
list_node_t *sorted_list_merge(list_node_t *pHead1, list_node_t *pHead2)
{
    if (!pHead1) {
        return pHead2;
    } else if (!pHead2) {
        return pHead1;
    }

    list_node_t *dummy = (list_node_t *)malloc(sizeof(list_node_t));
    list_node_t *head = dummy;

    while (pHead1 && pHead2) {
        if (pHead1->val < pHead2->val) {
            head->next = pHead1;
            pHead1 = pHead1->next;
        } else {
            head->next = pHead2;
            pHead2 = pHead2->next;
        }
        head = head->next;
    }

    if (pHead1) {
        head->next = pHead1;
    } else if (pHead2) {
        head->next = pHead2;
    }

    list_node_t *mergeHead = dummy->next;
    free(dummy);

    return mergeHead;
}

list_node_t *bsearchMerge(list_node_t **lists, int listLen, int left, int right) {
    if (left == right) {
        return lists[left];
    } else {
        int mid = left + (right - left) / 2;
        list_node_t *leftHead = bsearchMerge(lists, listLen, left, mid);
        list_node_t *rightHead = bsearchMerge(lists, listLen, mid + 1, right);
        list_node_t *newHead = (list_node_t*)malloc(sizeof(list_node_t));
        list_node_t *pre = newHead, *p1 = leftHead, *p2 = rightHead;
        while (p1 && p2) {
            if (p1->val < p2->val) {
                pre->next = p1;
                pre = p1;
                p1 = p1->next;
            } else {
                pre->next = p2;
                pre = p2;
                p2 = p2->next;
            }
        }
        if (p1) {
            pre->next = p1;
        } else if (p2) {
            pre->next = p2;
        }
        pre = newHead->next;
        free(newHead);
        return pre;
    }
}

// 合并k个已排序的链表
list_node_t *mergeSortedKLists(list_node_t **lists, int listsLen)
{
    if (listsLen == 0) {
        return NULL;
    }

    return bsearchMerge(lists, listsLen, 0, listsLen - 1);
}


// 单向链表的排序
list_node_t *sortInList(list_node_t *head)
{
    if (head == NULL || head->next == NULL) {
        return head;
    }

    list_node_t *mid = NULL;
    list_node_t *fast = head;
    list_node_t *slow = head;
    while (fast->next != NULL && fast->next->next != NULL) {
        slow = slow->next;
        fast = fast->next->next;
    }

    // 分割链表
    mid = slow->next;
    slow->next = NULL;

    slow = sortInList(head);
    fast = sortInList(mid);
    return sorted_list_merge(slow, fast);
}

// 链表中倒数最后k个结点
list_node_t *list_get_kth_from_tail(list_node_t *pHead, int k)
{
    if (!pHead) {
        return NULL;
    }

    list_node_t *fast = pHead;
    while (k > 0 && fast) {
        fast = fast->next;
        k--;
    }

    if (k > 0) {
        return NULL;
    }

    list_node_t *slow = pHead;
    while (fast) {
        slow = slow->next;
        fast = fast->next;
    }

    return slow;
}

static list_node_t *removeNth(list_node_t *head, int n, int *count)
{
    if (!head) {
        return NULL;
    }

    head->next = removeNth(head->next, n, count);
    (*count)++;

    if (*count == n) { // 当到倒数第n个结点时, 返回下一个结点
        list_node_t *node = head->next;
        free(head);
        return node;
    }

    return head;
}

// 删除链表的倒数第n个节点
list_node_t *removeNthFromEndRecursive(list_node_t *head, int n)
{
    int count = 0;
    return removeNth(head, n, &count);
}

list_node_t *createNode(int value)
{
    list_node_t * node = (list_node_t *)malloc(sizeof(list_node_t));
    if (!node) {
        printf("create list node failed.\n");
        return NULL;
    }

    node->next = NULL;
    node->val = value;

    return node;
}

list_node_t *list_remove_kth_from_end(list_node_t *head, int n)
{
    if (head == NULL) {
        return NULL;
    }

    list_node_t *dummy = createNode(0);
    dummy->next = head;

    list_node_t *fast = dummy;
    list_node_t *slow = dummy;

    for (int i = 0; i < n; i++) {
        if (fast->next == NULL) {
            free(dummy);
            return head;
        }
        fast = fast->next;
    }

    while (fast->next != NULL) {
        fast = fast->next;
        slow = slow->next;
    }

    list_node_t *toDelete = slow->next;
    slow->next = slow->next->next;
    free(toDelete);

    list_node_t *newHead = dummy->next;
    free(dummy);
    return newHead;
}