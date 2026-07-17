# Mystl 手写 C++ STL 实现 — 设计文档

> 对标 libstdc++，纯手写，全头文件实现（.h + .tcc）

## 1. 概述

### 1.1 目标

在 `learning/scaffold/cpp/stl/` 下，从头实现一套完整的 C++ 标准模板库（STL）教学实现，命名为 `mystl`（命名空间 `mystl`）。所有容器和算法均为纯手写底层数据结构，不依赖 `std::` 命名空间的任何容器或算法（仅使用语言核心特性）。

### 1.2 对标标准

- 接口严格对标 C++17 标准
- 文件组织对标 libstdc++（GCC 14.2.0）
- 实现风格：全头文件（模板类），.h 声明 + .tcc 实现（deque/list/forward_list）
- 命名空间：`mystl`，宏前缀 `MYSTL_`

### 1.3 设计原则

1. **学习导向** — 代码中嵌入中文注释，解释核心算法和数据结构原理
2. **标准兼容** — 接口命名、签名、行为与 `std::` 一致
3. **零外部依赖** — 仅依赖 C++ 语言核心特性，不使用任何 `std::` 容器
4. **正确性优先** — 先保证功能正确，再考虑性能优化

## 2. 文件结构

```
learning/scaffold/cpp/stl/
├── include/
│   └── mystl/
│       ├── mystl.h                 # 汇总头文件
│       ├── type_traits.h           # 类型萃取
│       ├── utility.h               # move/forward/swap/pair
│       ├── iterator.h              # 迭代器
│       ├── allocator.h             # 分配器 + allocator_traits
│       ├── functional.h            # 函数对象 + hash 特化
│       ├── memory.h                # 智能指针
│       ├── vector.h                # 动态数组（声明+实现全在 .h）
│       ├── deque.h                 # 双端队列（声明）
│       ├── deque.tcc               # deque 实现
│       ├── list.h                  # 双向链表（声明）
│       ├── list.tcc                # list 实现
│       ├── forward_list.h          # 单向链表（声明）
│       ├── forward_list.tcc        # forward_list 实现
│       ├── array.h                 # 定长数组
│       ├── stack.h                 # 栈适配器
│       ├── queue.h                 # 队列适配器
│       ├── priority_queue.h        # 优先队列
│       ├── rb_tree.h               # 红黑树（map/set 底层，全在 .h）
│       ├── map.h                   # 映射
│       ├── multimap.h              # 多重映射
│       ├── set.h                   # 集合
│       ├── multiset.h              # 多重集合
│       ├── hash_table.h            # 哈希表（unordered_* 底层，全在 .h）
│       ├── unordered_map.h         # 无序映射
│       ├── unordered_multimap.h    # 无序多重映射
│       ├── unordered_set.h         # 无序集合
│       ├── unordered_multiset.h    # 无序多重集合
│       ├── algorithm.h             # 算法（排序/查找/变换/堆）
│       └── numeric.h               # 数值算法
├── test/
│   ├── CMakeLists.txt
│   ├── type_traits_test.cpp
│   ├── utility_test.cpp
│   ├── iterator_test.cpp
│   ├── allocator_test.cpp
│   ├── vector_test.cpp
│   ├── deque_test.cpp
│   ├── list_test.cpp
│   ├── forward_list_test.cpp
│   ├── array_test.cpp
│   ├── stack_test.cpp
│   ├── queue_test.cpp
│   ├── priority_queue_test.cpp
│   ├── set_test.cpp
│   ├── multiset_test.cpp
│   ├── map_test.cpp
│   ├── multimap_test.cpp
│   ├── unordered_set_test.cpp
│   ├── unordered_multiset_test.cpp
│   ├── unordered_map_test.cpp
│   ├── unordered_multimap_test.cpp
│   ├── algorithm_test.cpp
│   └── numeric_test.cpp
├── CMakeLists.txt
├── Makefile
├── README.md
└── NOTES.md
```

## 3. 组件详解

### 3.1 基础设施（6 个组件）

#### 3.1.1 type_traits.h

对标 `<type_traits>`，提供编译期类型查询和变换。

**核心实现**：
- `integral_constant<T, v>` / `true_type` / `false_type`
- 复合类型查询：`is_same`、`is_void`、`is_integral`、`is_floating_point`、`is_array`、`is_pointer`、`is_reference`、`is_const`、`is_volatile`
- 类型变换：`remove_reference`、`remove_cv`、`remove_const`、`remove_volatile`、`remove_pointer`、`add_pointer`、`add_lvalue_reference`、`add_rvalue_reference`、`decay`
- 编译期选择：`conditional`、`enable_if`、`conjunction`、`disjunction`、`negation`
- 其他：`underlying_type`、`rank`、`extent`

**注**：`is_union` / `is_class` 需要编译器内置（`__is_union` / `__is_class`），否则只能给默认值。

#### 3.1.2 utility.h

对标 `<utility>`，提供通用工具。

**核心实现**：
- `pair<T1, T2>` — 异构对，含比较运算符、`piecewise_construct_t`
- `move(T&&)` — 右值转换
- `forward(T&&)` — 完美转发
- `swap(T&, T&)` — 交换
- `exchange(T&, U&&)` — 替换并返回旧值
- `integer_sequence` / `make_index_sequence` / `index_sequence_for`（C++14，但 STL 需要）
- `declval` — 不创建实例获取类型

#### 3.1.3 iterator.h

对标 `<iterator>`，提供迭代器基础设施。

**核心实现**：
- 迭代器标签：`input_iterator_tag`、`output_iterator_tag`、`forward_iterator_tag`、`bidirectional_iterator_tag`、`random_access_iterator_tag`
- `iterator_traits<T>` — 迭代器特性萃取（含指针特化 `T*`、`const T*`）
- `reverse_iterator<Iterator>` — 反向迭代器适配器
- `back_insert_iterator<Container>` / `front_insert_iterator<Container>` / `insert_iterator<Container>`
- 辅助函数：`distance`、`advance`、`next`、`prev`
- `istream_iterator<T>` / `ostream_iterator<T>`

#### 3.1.4 allocator.h

对标 `<memory>` 中的分配器部分。

**核心实现**：
- `allocator<T>` — 标准分配器
- `allocator_traits<Alloc>` — 分配器特性萃取
- `allocator<void>` 特化
- `construct_at` / `destroy_at`（C++17 或 C++20）
- `uninitialized_copy` / `uninitialized_fill` / `uninitialized_fill_n` / `uninitialized_move` / `uninitialized_default_construct` / `uninitialized_value_construct`

#### 3.1.5 functional.h

对标 `<functional>`，提供函数对象和哈希。

**核心实现**：
- 比较函数对象：`less<T>`、`greater<T>`、`equal_to<T>`、`not_equal_to<T>`、`less_equal<T>`、`greater_equal<T>`
- 算术函数对象：`plus<T>`、`minus<T>`、`multiplies<T>`、`divides<T>`、`modulus<T>`、`negate<T>`
- 逻辑函数对象：`logical_and<T>`、`logical_or<T>`、`logical_not<T>`
- `hash<T>` — 哈希函数对象特化（所有整数、浮点、指针、`const char*`）
- `reference_wrapper<T>` — 引用包装
- `identity` — 恒等函数（C++20，但很多实现需要）

#### 3.1.6 memory.h

对标 `<memory>` 中的智能指针部分。

**核心实现**：
- `default_delete<T>` — 默认删除器
- `unique_ptr<T, Deleter>` — 独占所有权智能指针（含数组特化 `T[]`）
- `shared_ptr<T>` — 共享所有权智能指针（引用计数）
- `weak_ptr<T>` — 弱引用（不增加引用计数）
- `make_unique<T>(Args&&...)` / `make_shared<T>(Args&&...)`
- `addressof(T&)` — 取地址（即使 `operator&` 被重载）

### 3.2 序列容器（5 个组件）

#### 3.2.1 vector.h

对标 `std::vector<T>`，动态数组。

**数据结构**：连续内存块，三指针（`start_` / `finish_` / `end_of_storage_`）

**迭代器**：`T*`（原生指针，因为元素连续存储）

**核心方法**（完整实现）：
- 构造/析构：默认、`n` 个、范围、拷贝、移动、初始化列表
- 容量：`size`、`capacity`、`empty`、`reserve`、`shrink_to_fit`、`max_size`
- 访问：`operator[]`、`at`、`front`、`back`、`data`
- 修改：`push_back`、`pop_back`、`emplace_back`、`insert`、`emplace`、`erase`、`clear`、`resize`、`swap`
- 比较：`==`、`!=`、`<`、`<=`、`>`、`>=`

**扩容策略**：容量不足时 2 倍增长（与 libstdc++ 一致）

**异常保证**：`push_back` / `emplace_back` 提供强异常保证（先分配再移动，移动失败回滚）

#### 3.2.2 deque.h + deque.tcc

对标 `std::deque<T>`，双端队列。

**数据结构**：中控器（map） + 分段连续缓冲区（block）

```
map_:  [block_ptr, block_ptr, block_ptr, ...]
         ↓          ↓          ↓
       [T...]    [T...]    [T...]    ← 每个 block 固定大小
```

**迭代器**：自定义 `deque_iterator`（含指向当前元素、当前 block 首尾、中控器位置的指针）

**核心方法**：
- `push_front` / `pop_front` — O(1)
- `push_back` / `pop_back` — O(1)
- `operator[]` / `at` — O(1)
- `insert` / `erase` — O(n)
- `resize` / `shrink_to_fit`

**迭代器失效规则**：与 libstdc++ 一致——首尾插入不失效，中间插入失效

#### 3.2.3 list.h + list.tcc

对标 `std::list<T>`，双向链表。

**数据结构**：带哨兵节点的双向循环链表

```
sentinel_ → [prev, next] ↔ [prev, next] ↔ [prev, next] ↔ ...
              ↑                                              ↑
              └──────────────────────────────────────────────┘
```

**迭代器**：自定义 `list_iterator`（包裹 `list_node_base*`，支持 `++`/`--`）

**核心方法**：
- `push_front` / `pop_front` — O(1)
- `push_back` / `pop_back` — O(1)
- `insert` / `erase` — O(1)
- `splice` — O(1) 转移节点
- `sort` — O(n log n) 归并排序
- `merge`、`reverse`、`unique`、`remove`、`remove_if`

**迭代器失效**：插入不失效，删除仅被删节点失效

#### 3.2.4 forward_list.h + forward_list.tcc

对标 `std::forward_list<T>`，单向链表。

**数据结构**：带哨兵节点的单向链表

```
before_begin_ → [next] → [next] → [next] → nullptr
```

**关键差异**（与 list 相比）：
- 使用 `insert_after` / `erase_after`（在指定位置**之后**操作）
- 有 `before_begin()` 迭代器
- **没有** `size()` 方法（O(1) 不维护 size）
- **没有** `push_back` / `pop_back`
- 迭代器只支持 `++`（不支持 `--`）

**核心方法**：
- `push_front` / `pop_front`
- `insert_after` / `emplace_after` / `erase_after`
- `splice_after`
- `sort` / `merge` / `reverse` / `unique`

#### 3.2.5 array.h

对标 `std::array<T, N>`，定长数组。

**数据结构**：`T data_[N]`（聚合类型）

**核心方法**：
- `begin` / `end` — 原生指针
- `operator[]` / `at` / `front` / `back` / `data`
- `fill` / `swap`
- `size` / `max_size` / `empty` — 编译期常量

**注意**：`array` 是聚合类型，没有用户声明的构造函数，支持聚合初始化 `mystl::array<int, 3> arr = {1, 2, 3};`

### 3.3 容器适配器（3 个组件）

#### 3.3.1 stack.h

对标 `std::stack<T, Container>`，后进先出适配器。

- 默认底层容器：`deque<T>`
- 接口：`top`、`push`、`pop`、`emplace`、`swap`
- 比较运算符：全

#### 3.3.2 queue.h

对标 `std::queue<T, Container>`，先进先出适配器。

- 默认底层容器：`deque<T>`
- 接口：`front`、`back`、`push`、`pop`、`emplace`、`swap`
- 比较运算符：全

#### 3.3.3 priority_queue.h

对标 `std::priority_queue<T, Container, Compare>`，最大堆适配器。

- 默认底层容器：`vector<T>`
- 默认比较：`less<T>`（最大堆）
- 接口：`top`、`push`、`pop`、`emplace`、`swap`
- 使用 `algorithm.h` 中的堆算法（`push_heap` / `pop_heap` / `make_heap`）

### 3.4 有序关联容器（4 + 1 个组件）

#### 3.4.1 rb_tree.h — 红黑树（内部实现）

对标 `std::_Rb_tree`（libstdc++ 内部类）。

**数据结构**：红黑树（带哨兵节点）

```cpp
struct rb_tree_node_base {
    rb_tree_node_base* parent;
    rb_tree_node_base* left;
    rb_tree_node_base* right;
    color_type color;  // red / black
};

template <typename T>
struct rb_tree_node : rb_tree_node_base {
    T value;
};
```

**红黑树性质**：
1. 每个节点是红色或黑色
2. 根节点是黑色
3. 叶子节点（哨兵）是黑色
4. 红色节点的子节点都是黑色
5. 从任一节点到其叶子的所有路径包含相同数量的黑色节点

**核心操作**：
- 插入（`insert_unique` / `insert_equal`）→ O(log n)
- 删除 → O(log n)
- 查找 → O(log n)
- 迭代器：中序遍历（`++` 找右子树最左，`--` 找左子树最右）

**迭代器**：自定义 `rb_tree_iterator`（包裹 `rb_tree_node_base*`）

**模板参数**：
```cpp
template <typename Key, typename Value, typename KeyOfValue,
          typename Compare, typename Alloc = allocator<Value>>
class rb_tree;
```

- `Key` — 键类型
- `Value` — 值类型（map 中为 `pair<const K, V>`，set 中为 `K`）
- `KeyOfValue` — 从 Value 提取 Key 的函数对象
- `Compare` — 比较函数对象

#### 3.4.2 set.h

对标 `std::set<T, Compare, Alloc>`。

- 底层：`rb_tree<T, T, identity<T>, Compare, Alloc>`
- 使用 `insert_unique`（不允许重复）
- 核心方法：`insert`、`erase`、`find`、`count`、`contains`、`lower_bound`、`upper_bound`、`equal_range`
- 迭代器：双向迭代器（中序遍历）

#### 3.4.3 multiset.h

对标 `std::multiset<T, Compare, Alloc>`。

- 底层：`rb_tree<T, T, identity<T>, Compare, Alloc>`
- 使用 `insert_equal`（允许重复）
- 核心方法同 set，但 `equal_range` 更为常用

#### 3.4.4 map.h

对标 `std::map<K, V, Compare, Alloc>`。

- 底层：`rb_tree<K, pair<const K, V>, select1st<pair<const K, V>>, Compare, Alloc>`
- 使用 `insert_unique`（不允许重复 key）
- 核心方法：`insert`、`erase`、`find`、`count`、`contains`、`at`、`operator[]`、`try_emplace`、`insert_or_assign`、`lower_bound`、`upper_bound`、`equal_range`

**`operator[]` 语义**：如果 key 不存在，创建默认值并返回引用

#### 3.4.5 multimap.h

对标 `std::multimap<K, V, Compare, Alloc>`。

- 底层：同 map，但使用 `insert_equal`
- **没有** `operator[]` 和 `at()`（因为 key 不唯一）
- `equal_range` 是核心操作

### 3.5 无序关联容器（4 + 1 个组件）

#### 3.5.1 hash_table.h — 哈希表（内部实现）

对标 libstdc++ 的 `_Hashtable`（在 `bits/hashtable.h` 中实现）。

**数据结构**：链地址法 + 桶数组

```cpp
template <typename T>
struct hash_node {
    T value;
    hash_node* next;
};

template <typename Key, typename Value, typename Hash, typename Pred,
          typename ExtractKey, typename Alloc>
class hash_table {
    vector<hash_node<Value>*> buckets_;  // 桶数组
    size_t size_;                        // 元素数量
    float max_load_factor_;              // 最大负载因子（默认 1.0）
    Hash hasher_;                        // 哈希函数
    Pred key_eq_;                        // 键相等判断
};
```

**核心操作**：
- `insert_unique` / `insert_equal`
- `erase` — 从桶中删除节点
- `find` — 计算 hash → 定位桶 → 链表中查找
- `rehash` — 负载因子超限时扩容（通常 2 倍）
- `reserve` — 预分配桶数

**迭代器**：前向迭代器（遍历所有桶 + 链表）

**模板参数**：
```cpp
template <typename Key, typename Value, typename Hash, typename Pred,
          typename ExtractKey, typename Alloc>
class hash_table;
```

#### 3.5.2 unordered_set.h

对标 `std::unordered_set<K, Hash, Pred, Alloc>`。

- 底层：`hash_table<K, K, Hash, Pred, identity<K>, Alloc>`
- 核心方法：`insert`、`erase`、`find`、`count`、`contains`、`rehash`、`reserve`
- 桶接口：`bucket_count`、`max_bucket_count`、`bucket_size`、`bucket`
- 负载因子：`load_factor`、`max_load_factor`

#### 3.5.3 unordered_multiset.h

对标 `std::unordered_multiset<K, Hash, Pred, Alloc>`。

- 底层：同 unordered_set，使用 `insert_equal`
- 允许重复元素

#### 3.5.4 unordered_map.h

对标 `std::unordered_map<K, V, Hash, Pred, Alloc>`。

- 底层：`hash_table<K, pair<const K, V>, Hash, Pred, select1st<pair<const K, V>>, Alloc>`
- 核心方法：`insert`、`erase`、`find`、`at`、`operator[]`、`try_emplace`、`insert_or_assign`
- 桶接口、负载因子同 unordered_set

#### 3.5.5 unordered_multimap.h

对标 `std::unordered_multimap<K, V, Hash, Pred, Alloc>`。

- 底层：同 unordered_map，使用 `insert_equal`
- **没有** `operator[]` 和 `at()`

### 3.6 算法（2 个组件）

#### 3.6.1 algorithm.h

对标 `<algorithm>`，约 60+ 函数。

**排序相关**：
- `sort(RandomIt, RandomIt, Compare)` — 快速排序 + 插入排序混合（introspective sort）
- `stable_sort` — 归并排序
- `partial_sort` / `nth_element` — 堆排序 + 快速选择
- `is_sorted` / `is_sorted_until`

**查找相关**：
- `find` / `find_if` / `find_if_not` / `find_end` / `find_first_of`
- `adjacent_find`
- `binary_search` / `lower_bound` / `upper_bound` / `equal_range`

**计数**：`count` / `count_if`

**变换**：`copy` / `copy_if` / `copy_n` / `move` / `transform` / `replace` / `replace_if` / `fill` / `fill_n` / `generate` / `generate_n`

**删除**：`remove` / `remove_if` / `remove_copy` / `unique` / `unique_copy`

**堆操作**：`make_heap` / `push_heap` / `pop_heap` / `sort_heap` / `is_heap`

**分区**：`partition` / `stable_partition` / `partition_point`

**极值**：`min` / `max` / `minmax` / `min_element` / `max_element` / `minmax_element`

**其他**：`swap` / `iter_swap` / `for_each` / `all_of` / `any_of` / `none_of` / `equal` / `lexicographical_compare`

#### 3.6.2 numeric.h

对标 `<numeric>`。

- `accumulate` — 累加
- `inner_product` — 内积
- `partial_sum` — 前缀和
- `adjacent_difference` — 相邻差
- `iota` — 递增填充
- `reduce`（C++17，并行版 accumulate，可简化为串行）

## 4. 实现顺序

建议分 6 个阶段实现，每个阶段可独立编译和测试：

### 阶段 1：基础设施（基础）
**组件**：`type_traits.h` → `utility.h` → `iterator.h` → `allocator.h` → `functional.h`
**目标**：所有上层容器依赖的基础都可用
**可测试**：`type_traits_test.cpp`、`utility_test.cpp`、`iterator_test.cpp`、`allocator_test.cpp`

### 阶段 2：序列容器（手感）
**组件**：`vector.h` → `array.h` → `list.h` + `list.tcc` → `forward_list.h` + `forward_list.tcc` → `deque.h` + `deque.tcc`
**目标**：核心序列容器可用，理解内存管理和迭代器
**可测试**：各容器独立测试 + 适配器测试

### 阶段 3：适配器（包装）
**组件**：`stack.h` → `queue.h` → `priority_queue.h`
**目标**：基于已有容器的适配器，简单但完整
**可测试**：适配器测试

### 阶段 4：红黑树（硬核）
**组件**：`rb_tree.h` → `set.h` → `multiset.h` → `map.h` → `multimap.h`
**目标**：最复杂的部分——红黑树实现，O(log n) 的有序关联容器
**可测试**：各容器独立测试，重点验证红黑树平衡性和迭代器正确性

### 阶段 5：哈希表（硬核）
**组件**：`hash_table.h` → `unordered_set.h` → `unordered_multiset.h` → `unordered_map.h` → `unordered_multimap.h`
**目标**：O(1) 平均的无序关联容器
**可测试**：各容器独立测试，重点验证哈希冲突处理和负载因子

### 阶段 6：算法（收尾）
**组件**：`algorithm.h` → `numeric.h` → `memory.h`
**目标**：泛型算法和智能指针
**可测试**：算法测试 + 整合测试

## 5. 命名约定

```cpp
// 命名空间
#define MYSTL_NAMESPACE_BEGIN namespace mystl {
#define MYSTL_NAMESPACE_END }

// 宏前缀
MYSTL_  // 所有宏以 MYSTL_ 开头

// 内部标识符
_mystl_  // 内部函数/变量前缀（类似 libstdc++ 的 _M_ / _S_）

// 文件守卫
MYSTL_VECTOR_H
MYSTL_LIST_H
// ...
```

## 6. 测试策略

- 每个组件一个独立测试文件
- 测试覆盖：
  - 正常操作（构造、插入、删除、查找）
  - 边界情况（空容器、单元素、满容量）
  - 迭代器遍历和失效
  - 比较运算符
  - 异常安全性（移动构造/赋值）
- 对标测试：相同用例在 `std::` 下运行验证行为一致

## 7. 参考资源

- **本地 libstdc++ 源码**：`C:/mingw64/include/c++/14.2.0/bits/`
  - `stl_vector.h` — vector 实现
  - `stl_deque.h` + `deque.tcc` — deque 实现
  - `stl_list.h` + `list.tcc` — list 实现
  - `stl_tree.h` — 红黑树实现
  - `hashtable.h` + `hashtable_policy.h` — 哈希表实现
  - `stl_map.h` / `stl_set.h` — map/set 包装
  - `stl_algo.h` — 算法实现
- **cppreference.com**：接口参考
- **《STL 源码剖析》（侯捷）**：中文学习参考

## 8. 附录：文件行数预估

| 文件 | 预估行数 | 说明 |
|------|---------|------|
| `type_traits.h` | ~400 | 模板元编程 |
| `utility.h` | ~200 | pair + move/forward |
| `iterator.h` | ~400 | 迭代器标签 + reverse_iterator + 适配器 |
| `allocator.h` | ~300 | 分配器 + uninitialized_* |
| `functional.h` | ~500 | 函数对象 + hash 特化 |
| `memory.h` | ~400 | 智能指针 |
| `vector.h` | ~600 | 动态数组 |
| `deque.h` + `.tcc` | ~300 + ~500 | 双端队列 |
| `list.h` + `.tcc` | ~300 + ~400 | 双向链表 |
| `forward_list.h` + `.tcc` | ~200 + ~300 | 单向链表 |
| `array.h` | ~100 | 定长数组 |
| `stack.h` | ~80 | 适配器 |
| `queue.h` | ~80 | 适配器 |
| `priority_queue.h` | ~100 | 堆适配器 |
| `rb_tree.h` | ~800 | 红黑树（最复杂） |
| `set.h` | ~100 | 包装 |
| `multiset.h` | ~80 | 包装 |
| `map.h` | ~200 | 包装 + operator[] |
| `multimap.h` | ~80 | 包装 |
| `hash_table.h` | ~1000 | 哈希表（最复杂） |
| `unordered_set.h` | ~150 | 包装 |
| `unordered_multiset.h` | ~100 | 包装 |
| `unordered_map.h` | ~200 | 包装 |
| `unordered_multimap.h` | ~100 | 包装 |
| `algorithm.h` | ~1500 | 60+ 算法函数 |
| `numeric.h` | ~200 | 数值算法 |
| **总计** | **~8500** | |