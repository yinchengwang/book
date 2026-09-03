/*
 * mystl.h - Mystl 头文件汇总
 *
 * 包含所有 mystl 组件的一站式头文件
 */

#ifndef MYSTL_MYSTL_H
#define MYSTL_MYSTL_H

// 命名空间开始
#define MYSTL_NAMESPACE_BEGIN namespace mystl {
#define MYSTL_NAMESPACE_END }

// 底层基础设施
#include "type_traits.h"
#include "utility.h"
#include "iterator.h"
#include "allocator.h"
#include "functional.h"
#include "memory.h"

// 容器
#include "array.h"
#include "vector.h"
#include "list.h"
#include "forward_list.h"
#include "deque.h"
#include "stack.h"
#include "queue.h"
#include "priority_queue.h"
#include "set.h"
#include "multiset.h"
#include "map.h"
#include "multimap.h"
#include "unordered_set.h"
#include "unordered_map.h"
#include "unordered_multiset.h"
#include "unordered_multimap.h"

// 算法
#include "algorithm.h"
#include "numeric.h"

#endif // MYSTL_MYSTL_H