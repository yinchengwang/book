# stl 学习笔记

## 概念地图

STL（Standard Template Library）是 C++ 标准库的"模板化部分"——容器 + 迭代器 + 算法 + 函数对象的四件套：

- **四大组件**：
  - **容器（Container）**：存储数据的类模板（vector、map 等）
  - **迭代器（Iterator）**：访问容器元素的"通用指针"——算法通过迭代器操作容器
  - **算法（Algorithm）**：与容器解耦的通用函数（sort、find 等）
  - **函数对象（Functor）**：可调用对象——lambda、函数指针、struct operator()
- **容器分类**：
  - **序列容器**：`vector`（连续、随机访问 O(1)、尾部插入 O(1)）、`list`（双向链表、任意位置插入 O(1)）、`deque`（双端队列）
  - **关联容器**（有序，O(log n)）：`map` / `set` / `multimap` / `multiset`——红黑树
  - **无序容器**（O(1) 平均）：`unordered_map` / `unordered_set`——哈希表
  - **容器适配器**：`stack` / `queue` / `priority_queue`
- **`<algorithm>` 60+ 函数**：排序（sort/stable_sort/partial_sort/nth_element）、查找（find/find_if/any_of/all_of）、计数（count/count_if）、变换（transform/copy/replace/fill）、分区（partition/stable_partition）、堆操作（make_heap/push_heap/pop_heap）
- **`<numeric>` 函数**：`accumulate` / `partial_sum` / `inner_product` / `iota` / `adjacent_difference`
- **Lambda 表达式**：C++11 起，`[capture](params){ body }` ——配合 STL 算法威力最大
- **结构化绑定（C++17）**：`auto [a, b] = pair;` ——pair/tuple/struct 解包

## 踩坑记录

1. **迭代器失效**：`vector` 扩容后所有迭代器失效；`list` 插入不会失效
2. **`map` 找不到返回 `end()`**：`auto it = m.find(k); if (it != m.end()) ...`
3. **`operator[]` 自动插入**：`m["nonexistent"]` 会插入默认值——**用 `find`/`contains` 检查存在性**
4. **`sort` 仅支持随机访问迭代器**：`list` 不能用 `sort`，要用 `list.sort()`
5. **`unordered_map` 哈希冲突**：自定义 key 需提供 `std::hash<Key>` 特化
6. **Lambda 捕获**：`[=]` 值捕获、`[&]` 引用捕获、`[this]` 捕获 this 指针
7. **`auto` 推导**：Lambda 返回 `std::function` 还是闭包类型？**用 `auto` 保留闭包类型更高效**

## 工程对照（≥100 字硬约束）

STL 在 `engineering/` 中体现为"手写数据结构 vs STL 容器"的对比：

1. **`engineering/src/algo-prod/sort/sort.c` 手写排序**：纯 C 风格——`qsort(arr, n, sizeof(int), cmp)` + 函数指针。C++ 用 `std::sort(vec.begin(), vec.end())` ——更简洁更安全
2. **`engineering/src/db/index/vector_index/hnsw/hnsw_search.c` 优先级队列**：手写堆实现 `heap_push`/`heap_pop`。C++ 等价物：`std::priority_queue<std::pair<float, int>>`——自动管理
3. **`engineering/src/db/storage/bufmgr.c` Buffer Pool 链表**：C 风格用 `list_for_each` 遍历。C++ 用 `std::list<Page*>` 或 `std::vector<std::unique_ptr<Page>>`
4. **`engineering/src/db/storage/page/disk.c` 页表**：`Page* pages[MAX_PAGES]` 固定数组。C++ 等价物：`std::array<std::unique_ptr<Page>, MAX_PAGES>`
5. **`engineering/src/db/index/vector_index/BM25/bm25_search.c` 倒排索引**：`PostingList` 自定义结构。C++ 等价物：`std::unordered_map<int, std::vector<int>>`——开箱即用
6. **`engineering/src/db/api/handlers.c` API 路由**：`Route routes[N]` + 线性查找。C++ 用 `std::unordered_map<std::string, Handler>`——O(1) 查找
7. **`engineering/src/algo-prod/dict/dict_hmm.c` HMM 状态**：`double trans[N][N]` 转移矩阵。C++ 用 `std::vector<std::vector<double>>` 或 `std::array<std::array<double, N>, N>`

学完本卡能动手的事：把 `learning/scaffold/sort_basic/main.c` 的 qsort 改写为 C++ `std::sort`，对比两者代码量与可维护性。