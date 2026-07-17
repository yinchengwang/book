/*
 * ds_err.h - 数据结构库错误码（学习轨道）
 *
 * 学习轨道保持独立，不依赖工程轨道的 base_err.h。
 * 错误码命名规范: DS_<级别>_<描述>
 * 级别: WARN / ERROR / FATAL
 */
#ifndef DS_DS_ERR_H
#define DS_DS_ERR_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Queue 错误码
// ============================================================

#define DS_ERROR_QUEUE_EMPTY        "DS_ERROR_QUEUE_EMPTY"        // 队列为空
#define DS_ERROR_QUEUE_FULL         "DS_ERROR_QUEUE_FULL"         // 队列已满
#define DS_ERROR_QUEUE_NOMEM        "DS_ERROR_QUEUE_NOMEM"        // 队列内存分配失败

// ============================================================
// Stack 错误码
// ============================================================

#define DS_ERROR_STACK_EMPTY        "DS_ERROR_STACK_EMPTY"        // 栈为空
#define DS_ERROR_STACK_FULL         "DS_ERROR_STACK_FULL"         // 栈已满
#define DS_ERROR_STACK_NOMEM        "DS_ERROR_STACK_NOMEM"        // 栈内存分配失败

// ============================================================
// List 错误码
// ============================================================

#define DS_ERROR_LIST_EMPTY         "DS_ERROR_LIST_EMPTY"         // 链表为空
#define DS_ERROR_LIST_NOT_FOUND     "DS_ERROR_LIST_NOT_FOUND"     // 元素未找到
#define DS_ERROR_LIST_NOMEM         "DS_ERROR_LIST_NOMEM"         // 链表内存分配失败

// ============================================================
// Tree 错误码
// ============================================================

#define DS_ERROR_TREE_EMPTY         "DS_ERROR_TREE_EMPTY"         // 树为空
#define DS_ERROR_TREE_NOT_FOUND     "DS_ERROR_TREE_NOT_FOUND"     // 节点未找到
#define DS_ERROR_TREE_DUPLICATE     "DS_ERROR_TREE_DUPLICATE"     // 重复键
#define DS_ERROR_TREE_NOMEM         "DS_ERROR_TREE_NOMEM"         // 树内存分配失败
#define DS_ERROR_TREE_UNBALANCED    "DS_ERROR_TREE_UNBALANCED"    // 树不平衡

// ============================================================
// Heap 错误码
// ============================================================

#define DS_ERROR_HEAP_EMPTY         "DS_ERROR_HEAP_EMPTY"         // 堆为空
#define DS_ERROR_HEAP_NOMEM         "DS_ERROR_HEAP_NOMEM"         // 堆内存分配失败

// ============================================================
// String 错误码
// ============================================================

#define DS_ERROR_STRING_EMPTY       "DS_ERROR_STRING_EMPTY"       // 字符串为空
#define DS_ERROR_STRING_TOO_LONG    "DS_ERROR_STRING_TOO_LONG"    // 字符串过长
#define DS_ERROR_STRING_INVALID_ENCODING "DS_ERROR_STRING_INVALID_ENCODING"  // 无效编码
#define DS_ERROR_STRING_NOMEM       "DS_ERROR_STRING_NOMEM"       // 字符串内存分配失败

// ============================================================
// 通用错误码
// ============================================================

#define DS_ERROR_INVALID_CAPACITY   "DS_ERROR_INVALID_CAPACITY"   // 无效容量
#define DS_ERROR_INVALID_INDEX      "DS_ERROR_INVALID_INDEX"      // 无效索引
#define DS_ERROR_INVALID_TYPE       "DS_ERROR_INVALID_TYPE"       // 无效类型
#define DS_ERROR_NOT_IMPLEMENTED    "DS_ERROR_NOT_IMPLEMENTED"    // 功能未实现
#define DS_ERROR_OPERATION_FAILED   "DS_ERROR_OPERATION_FAILED"   // 操作失败

#ifdef __cplusplus
}
#endif

#endif // DS_DS_ERR_H
