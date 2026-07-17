// stl scaffold — STL 容器与算法
//
// 复现命令：
//   g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
//
// 演示 5 段：
//   [vector]      — std::vector 动态数组
//   [map-set]     — std::map / std::unordered_map / std::set
//   [algorithm]   — std::sort / find / count / transform / for_each
//   [lambda]      — 配合 lambda 的 STL 算法
//   [pair-tuple]  — std::pair / std::tuple / 结构化绑定

#include <iostream>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <string>
#include <numeric>
#include <cstdio>

int main() {
    // === [vector] std::vector 动态数组 ===
    printf("[vector] === std::vector ===\n");
    std::vector<int> v = {5, 2, 8, 1, 9, 3};
    printf("  size=%zu, capacity=%zu\n", v.size(), v.capacity());

    v.push_back(7);
    v.insert(v.begin() + 1, 100);
    printf("  push_back(7), insert(100): ");
    for (int x : v) printf("%d ", x);
    printf("\n");

    v.pop_back();
    printf("  pop_back(): size=%zu\n", v.size());

    // === [map-set] map / unordered_map / set ===
    printf("\n[map-set] === std::map / std::unordered_map / std::set ===\n");

    // std::map（红黑树，有序）
    std::map<std::string, int> ages = {{"alice", 30}, {"bob", 25}, {"carol", 28}};
    ages["david"] = 35;
    printf("  std::map (有序):\n");
    for (const auto& [name, age] : ages) {     // C++17 结构化绑定
        printf("    %s -> %d\n", name.c_str(), age);
    }

    // std::unordered_map（哈希，O(1)）
    std::unordered_map<std::string, int> cache = {{"a", 1}, {"b", 2}};
    cache["c"] = 3;
    printf("  std::unordered_map (哈希):\n");
    for (const auto& [k, v] : cache) {
        printf("    %s -> %d\n", k.c_str(), v);
    }

    // std::set
    std::set<int> s = {3, 1, 4, 1, 5, 9, 2, 6};   // 自动去重+排序
    printf("  std::set (有序去重): ");
    for (int x : s) printf("%d ", x);
    printf("\n");

    // === [algorithm] <algorithm> 头文件 ===
    printf("\n[algorithm] === <algorithm> 头文件 ===\n");

    std::vector<int> nums = {5, 2, 8, 1, 9, 3};

    // sort
    std::sort(nums.begin(), nums.end());
    printf("  sort: ");
    for (int x : nums) printf("%d ", x);
    printf("\n");

    // find
    auto it = std::find(nums.begin(), nums.end(), 5);
    printf("  find(5): %s (offset %ld)\n",
           it != nums.end() ? "found" : "not found",
           it != nums.end() ? (long)(it - nums.begin()) : -1L);

    // count
    std::vector<int> dup = {1, 2, 2, 3, 2, 4};
    int n2 = std::count(dup.begin(), dup.end(), 2);
    printf("  count(2) in dup: %d\n", n2);

    // accumulate (numeric 头)
    int sum = std::accumulate(nums.begin(), nums.end(), 0);
    printf("  accumulate: %d\n", sum);

    // min_element / max_element
    auto [min_it, max_it] = std::minmax_element(nums.begin(), nums.end());
    printf("  min=%d, max=%d\n", *min_it, *max_it);

    // === [lambda] lambda 配合算法 ===
    printf("\n[lambda] === lambda 配合算法 ===\n");
    std::vector<int> data = {1, 2, 3, 4, 5};

    // transform: 平方
    std::vector<int> squares(data.size());
    std::transform(data.begin(), data.end(), squares.begin(),
                   [](int x) { return x * x; });
    printf("  transform (平方): ");
    for (int x : squares) printf("%d ", x);
    printf("\n");

    // for_each: 打印
    printf("  for_each: ");
    std::for_each(data.begin(), data.end(), [](int x) { printf("%d ", x); });
    printf("\n");

    // sort with lambda (自定义比较)
    std::vector<int> mixed = {5, -2, 8, -1, 9};
    std::sort(mixed.begin(), mixed.end(), [](int a, int b) {
        return std::abs(a) < std::abs(b);   // 按绝对值升序
    });
    printf("  sort by abs: ");
    for (int x : mixed) printf("%d ", x);
    printf("\n");

    // === [pair-tuple] pair / tuple ===
    printf("\n[pair-tuple] === std::pair / std::tuple ===\n");
    std::pair<int, std::string> p = {42, "answer"};
    printf("  pair<int,string>: (%d, %s)\n", p.first, p.second.c_str());

    std::tuple<int, double, std::string> t = {1, 3.14, "hello"};
    printf("  tuple: (%d, %.2f, %s)\n",
           std::get<0>(t), std::get<1>(t), std::get<2>(t).c_str());

    // 结构化绑定（C++17）
    auto [i, d, s_str] = t;
    printf("  结构化绑定: i=%d, d=%.2f, s=%s\n", i, d, s_str.c_str());

    printf("\n=== PASS ===\n");
    return 0;
}