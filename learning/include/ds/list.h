/*
 * list.h - 单链表 LeetCode 题解
 *
 * LeetCode 链表题解的头文件，包含链表节点定义和所有 LeetCode 题解函数。
 *
 * 实现文件: src/ds/linked_list/impl/single_list.c
 */
#ifndef DS_LIST_H
#define DS_LIST_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 链表节点定义
// ============================================================

typedef struct list_node {
    int val;
    struct list_node *next;
} list_node_t;

// ============================================================
// 基础操作
// ============================================================

/**
 * @brief 从数组创建链表（尾插法）
 * @param arr 输入数组
 * @param size 数组大小
 * @return 链表头节点
 */
extern list_node_t *list_create(int *arr, int size);

/**
 * @brief 销毁链表（释放所有节点）
 * @param head 链表头节点
 */
extern void list_destroy(list_node_t *head);

/**
 * @brief 头插法
 * @param head 指向头指针的指针
 * @param val 要插入的值
 */
extern void list_insert_head(list_node_t **head, int val);

/**
 * @brief 尾插法
 * @param head 指向头指针的指针
 * @param val 要插入的值
 */
extern void list_insert_tail(list_node_t **head, int val);

// ============================================================
// 链表操作
// ============================================================

/**
 * @brief 获取链表尾节点
 */
extern list_node_t *list_get_tail(list_node_t *head);

/**
 * @brief 获取第 k 个节点
 */
extern list_node_t *list_get_kth_node(list_node_t *head, uint32_t k);

/**
 * @brief 获取链表长度
 */
extern uint32_t list_get_length(list_node_t *head);

/**
 * @brief 反转链表
 */
extern list_node_t *list_reverse(list_node_t *head);

/**
 * @brief 反转链表 m-n 部分
 */
extern list_node_t *reverse_list_between(list_node_t *head, int m, int n);

/**
 * @brief K 个一组反转链表
 */
extern list_node_t *list_reverse_k_group(list_node_t *head, int k);

/**
 * @brief 两数相加（链表逆序存储数字）
 */
extern list_node_t *list_node_add(list_node_t *head1, list_node_t *head2);

/**
 * @brief 判断链表是否为回文
 */
extern bool is_list_pail(list_node_t *head);

/**
 * @brief 获取两个链表的第一个公共节点
 */
extern list_node_t *list_first_common_node(list_node_t *pHead1, list_node_t *pHead2);

/**
 * @brief 奇偶链表（将奇数节点放在偶数节点前面）
 */
extern list_node_t *odd_even_list(list_node_t *head);

/**
 * @brief 删除链表中的重复节点（保留一个）
 */
extern list_node_t *list_duplicates(list_node_t *head);

/**
 * @brief 删除链表中的重复节点（不保留）
 */
extern list_node_t *list_delete_duplicates(list_node_t *head);

// ============================================================
// 链表环检测
// ============================================================

/**
 * @brief 判断链表是否有环
 */
extern bool list_has_cycle(list_node_t *head);

/**
 * @brief 获取链表环的入口节点
 */
extern list_node_t *list_cycle_entry(list_node_t *pHead);

// ============================================================
// 合并与排序
// ============================================================

/**
 * @brief 合并两个有序链表
 */
extern list_node_t *sorted_list_merge(list_node_t *pHead1, list_node_t *pHead2);

/**
 * @brief 合并 K 个有序链表
 */
extern list_node_t *mergeSortedKLists(list_node_t **lists, int listsLen);

/**
 * @brief 在链表中排序（归并排序）
 */
extern list_node_t *sortInList(list_node_t *head);

// ============================================================
// 删除相关
// ============================================================

/**
 * @brief 返回倒数第 k 个节点
 */
extern list_node_t *list_get_kth_from_tail(list_node_t *pHead, int k);

/**
 * @brief 删除链表的倒数第 n 个节点（递归版本）
 */
extern list_node_t *removeNthFromEndRecursive(list_node_t *head, int n);

/**
 * @brief 删除链表的倒数第 n 个节点
 */
extern list_node_t *list_remove_kth_from_end(list_node_t *head, int n);

#ifdef __cplusplus
}
#endif

#endif // DS_LIST_H
