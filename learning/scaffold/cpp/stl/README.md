# stl scaffold

C++ STL 容器与算法演示——vector/list/map/unordered_map/set + sort/find/count/transform/for_each + lambda + pair/tuple。

## 复现命令

```bash
cd learning/scaffold/stl

g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
```

## 关键点

- **容器三剑客**：
  - 序列：`vector` / `list` / `deque` / `array` / `forward_list`
  - 关联（有序）：`map` / `set` / `multimap` / `multiset`（红黑树）
  - 无序（哈希）：`unordered_map` / `unordered_set`（O(1) 平均）
- **算法头 `<algorithm>`**：
  - 排序：`sort` / `stable_sort` / `partial_sort` / `nth_element`
  - 查找：`find` / `find_if` / `binary_search` / `lower_bound`
  - 计数：`count` / `count_if`
  - 变换：`transform` / `copy` / `replace` / `fill`
  - 遍历：`for_each`
- **`<numeric>` 头**：`accumulate` / `partial_sum` / `inner_product` / `iota`
- **`std::pair` / `std::tuple`**：异构容器——`pair<K, V>` / `tuple<T1, T2, ...>`
- **结构化绑定（C++17）**：`auto [a, b, c] = tuple;` ——优雅解包

详见 NOTES.md 工程对照段。