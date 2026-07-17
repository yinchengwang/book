# 目录重组任务清单

## 1. 创建目录结构

- [x] 1.1 创建 `src/ds/` 目录结构（array, linked_list, stack, queue, tree, hash, graph, string, bit, segment_tree, fenwick_tree, bitmap, cache）
- [x] 1.2 创建 `src/ds/tree/` 子目录（binary_tree, bst, avl, rb_tree, b_tree, trie, heap, interval_tree）
- [x] 1.3 创建 `include/ds/` 目录结构（与 src/ds/ 镜像）
- [x] 1.4 创建 `test/ds/` 目录结构（与 src/ds/ 镜像）
- [x] 1.5 创建 `src/algo/` 补充目录（sliding_window, two_pointers, dp, greedy, backtrack, prefix_sum, divide_conquer, recursion, kmp, monotonic, bit_operations）

## 2. 迁移 self_made 数据结构到 ds/

- [x] 2.1 迁移 `src/self_made/heap/` → `src/ds/tree/heap/impl/`
- [x] 2.2 迁移 `src/self_made/list/` → `src/ds/linked_list/impl/`
- [x] 2.3 迁移 `src/self_made/queue/` → `src/ds/queue/impl/`
- [x] 2.4 迁移 `src/self_made/stack/` → `src/ds/stack/impl/`
- [x] 2.5 迁移 `src/self_made/tree/` → `src/ds/tree/<各子分类>/impl/`
- [x] 2.6 迁移 `src/self_made/str/` → `src/ds/string/impl/`
- [x] 2.7 迁移 `src/self_made/sort/` → `src/algo/sorting/impl/`
- [x] 2.8 迁移 `src/self_made/common/` → `src/ds/` (工具函数)

## 3. 迁移 algo/ds 数据结构到 ds/

- [x] 3.1 迁移 `src/algo/ds/array.c` → `src/ds/array/impl/`
- [x] 3.2 迁移 `src/algo/ds/linked_list.c` → `src/ds/linked_list/impl/`
- [x] 3.3 迁移 `src/algo/ds/binary_tree.c` → `src/ds/tree/binary_tree/impl/`
- [x] 3.4 迁移 `src/algo/ds/bst.c` → `src/ds/tree/bst/impl/`
- [x] 3.5 迁移 `src/algo/ds/heap.c` → `src/ds/tree/heap/impl/`
- [x] 3.6 迁移 `src/algo/ds/hash_table.c` → `src/ds/hash/impl/`
- [x] 3.7 迁移 `src/algo/ds/graph.c` → `src/ds/graph/impl/`
- [x] 3.8 迁移 `src/algo/ds/queue.c` → `src/ds/queue/impl/`
- [x] 3.9 迁移 `src/algo/ds/deque.c` → `src/ds/queue/impl/`
- [x] 3.10 迁移 `src/algo/ds/bitmap.c` → `src/ds/bitmap/impl/`
- [x] 3.11 迁移 `src/algo/ds/segment_tree.c` → `src/ds/segment_tree/impl/`
- [x] 3.12 迁移 `src/algo/ds/fenwick_tree.c` → `src/ds/fenwick_tree/impl/`
- [x] 3.13 迁移 `src/algo/ds/lru_cache.c` → `src/ds/cache/impl/`
- [x] 3.14 迁移 `src/algo/ds/lfu_cache.c` → `src/ds/cache/impl/`
- [x] 3.15 迁移 `src/algo/ds/monotonic_stack.c` → `src/ds/stack/impl/`
- [x] 3.16 迁移 `src/algo/ds/monotonic_queue.c` → `src/ds/queue/impl/`
- [x] 3.17 迁移 `src/algo/ds/trie.c` → `src/ds/tree/trie/impl/`
- [x] 3.18 迁移 `src/algo/ds/recursion.c` → `src/algo/recursion/impl/`

## 4. 重组 algo 目录

- [x] 4.1 重组 `src/algo/sorting/` → 保持，补充 `impl/` 和 `problems/`
- [x] 4.2 重组 `src/algo/binary_search/` → 保持，补充 `impl/` 和 `problems/`
- [x] 4.3 补充 `src/algo/sliding_window/problems/`
- [x] 4.4 补充 `src/algo/dp/problems/`
- [x] 4.5 补充 `src/algo/greedy/problems/`
- [x] 4.6 补充 `src/algo/backtrack/problems/`
- [x] 4.7 补充 `src/algo/prefix_sum/problems/`
- [x] 4.8 补充 `src/algo/two_pointers/problems/`

## 5. 迁移 LeetCode 题目到对应分类

### 数组类题目
- [x] 5.1 迁移 `leetcode_80/` → `src/ds/array/problems/0080_remove_duplicates/`
- [x] 5.2 迁移 `leetcode_1338/` → `src/ds/array/problems/0133_reduce_array/`
- [x] 5.3 迁移 `leetcode_1493/` → `src/ds/array/problems/1493_longest_one_row/`
- [x] 5.4 迁移 `leetcode_1705/` → `src/ds/tree/heap/problems/1705_eaten_apples/` 和 `src/ds/array/problems/`
- [x] 5.5 迁移 `leetcode_1984/` → `src/algo/sliding_window/problems/1984_minimum_difference/`
- [x] 5.6 迁移 `leetcode_2006/` → `src/ds/array/problems/2006_count_pairs/`
- [x] 5.7 迁移 `leetcode_2270/` → `src/algo/sliding_window/problems/2270_ways_to_split/`
- [x] 5.8 迁移 `leetcode_2275/` → `src/ds/bit/problems/2275_bit_and/`
- [x] 5.9 迁移 `leetcode_2469/` → `src/ds/array/problems/2469_convert_temperature/`
- [x] 5.10 迁移 `leetcode_2595/` → `src/ds/bit/problems/2595_even_odd_bit/`
- [x] 5.11 迁移 `leetcode_3046/` → `src/ds/array/problems/3046_split_array/`
- [x] 5.12 迁移 `leetcode_3065/` → `src/ds/array/problems/3065_min_operations/`

### 链表类题目
- [x] 5.13 迁移 `leetcode_19/` → `src/ds/linked_list/problems/0019_remove_nth/`
- [x] 5.14 迁移 `leetcode_142/` → `src/ds/linked_list/problems/0142_detect_cycle/`
- [x] 5.15 迁移 `leetcode_143/` → `src/ds/linked_list/problems/0143_reorder_list/`
- [x] 5.16 迁移 `leetcode_206/` → `src/ds/linked_list/problems/0206_reverse_list/`
- [x] 5.17 迁移 `leetcode_445/` → `src/ds/linked_list/problems/0445_add_two_numbers/`

### 栈类题目
- [x] 5.18 迁移 `leetcode_20/` → `src/ds/stack/problems/0020_valid_parentheses/`

### 二叉树类题目
- [x] 5.19 迁移 `leetcode_100/` → `src/ds/tree/binary_tree/problems/0100_same_tree/`
- [x] 5.20 迁移 `leetcode_222/` → `src/ds/tree/binary_tree/problems/0222_count_complete/`

### 堆类题目
- [x] 5.21 迁移 `leetcode_3066/` → `src/ds/tree/heap/problems/3066_min_operations/`

### 哈希表类题目
- [x] 5.22 迁移 `leetcode_1366/` → `src/ds/hash/problems/1366_rank_teams/`
- [x] 5.23 迁移 `leetcode_1367/` → `src/ds/hash/problems/1367_linked_list_tree/`

### 字符串类题目
- [x] 5.24 迁移 `leetcode_551/` → `src/ds/string/problems/0551_student_attendance/`
- [x] 5.25 迁移 `leetcode_1941/` → `src/ds/string/problems/1941_same_occurrences/`
- [x] 5.26 迁移 `leetcode_2264/` → `src/ds/string/problems/2264_largest_good/`
- [x] 5.27 迁移 `leetcode_3019/` → `src/ds/string/problems/3019_key_changes/`

### 二分类目
- [x] 5.28 迁移 `leetcode_2070/` → `src/algo/binary_search/problems/2070_magic_list/`
- [x] 5.29 迁移 `leetcode_3280/` → `src/algo/binary_search/problems/3280_count_days/`

### 前缀和类题目
- [x] 5.30 迁移 `leetcode_3159/` → `src/algo/prefix_sum/problems/3159_query_results/`

### 其他题目
- [x] 5.31 迁移 `interview_17_10/` → `src/interview/` (保留原目录)
- [x] 5.32 迁移 `a_match/` → `src/algo/contest/` (保留原目录)

## 6. 创建题目 README.md

- [x] 6.1 为数组类题目创建 README.md（含题解说明）
- [x] 6.2 为链表类题目创建 README.md
- [x] 6.3 为栈类题目创建 README.md
- [x] 6.4 为二叉树类题目创建 README.md
- [x] 6.5 为哈希表类题目创建 README.md
- [x] 6.6 为字符串类题目创建 README.md
- [x] 6.7 为二分查找类题目创建 README.md
- [x] 6.8 为滑动窗口类题目创建 README.md
- [x] 6.9 为前缀和类题目创建 README.md

## 7. 创建数据结构 README.md

- [x] 7.1 创建 `src/ds/array/README.md`（含 Mermaid 可视化）
- [x] 7.2 创建 `src/ds/linked_list/README.md`（含 Mermaid 可视化）
- [x] 7.3 创建 `src/ds/stack/README.md`（含 Mermaid 可视化）
- [x] 7.4 创建 `src/ds/queue/README.md`（含 Mermaid 可视化）
- [x] 7.5 创建 `src/ds/tree/binary_tree/README.md`（含 Mermaid 可视化）
- [x] 7.6 创建 `src/ds/tree/heap/README.md`（含 Mermaid 可视化）
- [x] 7.7 创建 `src/ds/hash/README.md`（含 Mermaid 可视化）
- [x] 7.8 创建 `src/ds/string/README.md`（含 Mermaid 可视化）
- [x] 7.9 创建 `src/ds/bit/README.md`（含 Mermaid 可视化）
- [x] 7.10 创建其他数据结构 README.md

## 8. 创建算法 README.md

- [x] 8.1 创建 `src/algo/sorting/README.md`（含 Mermaid 可视化）
- [x] 8.2 创建 `src/algo/binary_search/README.md`（含 Mermaid 可视化）
- [x] 8.3 创建 `src/algo/sliding_window/README.md`（含 Mermaid 可视化）
- [x] 8.4 创建 `src/algo/dp/README.md`（含 Mermaid 可视化）
- [x] 8.5 创建 `src/algo/prefix_sum/README.md`（含 Mermaid 可视化）
- [x] 8.6 创建其他算法 README.md

## 9. 更新 CMakeLists.txt

- [x] 9.1 更新 `src/CMakeLists.txt` 添加 `ds/` 和 `algo/` 目录
- [x] 9.2 创建 `src/ds/CMakeLists.txt`
- [ ] 9.3 为每个 `src/ds/<分类>/` 创建 CMakeLists.txt
- [ ] 9.4 为每个 `src/algo/<分类>/` 创建 CMakeLists.txt
- [x] 9.5 创建 `include/ds/CMakeLists.txt`
- [x] 9.6 创建 `include/algo/CMakeLists.txt`

## 10. 更新 test 目录

- [ ] 10.1 重组 `test/self_made/` → `test/ds/<分类>/`
- [ ] 10.2 重组 `test/algo/` → `test/ds/<分类>/` 或 `test/algo/<分类>/`
- [ ] 10.3 更新测试 CMakeLists.txt

## 11. 删除过渡目录

- [ ] 11.1 验证构建通过后删除 `src/self_made/`
- [x] 11.2 验证构建通过后删除 `src/algo/ds/`
- [ ] 11.3 验证构建通过后删除 `test/self_made/`

## 12. 最终验证

- [x] 12.1 运行 `cmake --build .` 验证构建通过
- [ ] 12.2 运行 `ctest` 验证测试通过
- [x] 12.3 检查所有 README.md 是否正确渲染
