# 红黑树工程对照说明

## 教学代码 vs 工程实现

本节的 `main.c` 是教学级别的红黑树实现，展示了核心算法逻辑。实际工程中，红黑树通常作为标准库或成熟框架的底层数据结构，而非手写。

## STL map 底层实现

C++ STL 的 `std::map` 和 `std::multimap` 底层采用 **红黑树** 实现（gcc/libstdc++ 中为 `std::_Rb_tree`）。

### 关键工程特性

| 特性 | 说明 |
|------|------|
| 模板参数 | `Key, T, Compare = std::less<Key>`，支持自定义比较函数 |
| 迭代器 | BidirectionalIterator，支持 ++/-- 双向遍历 |
| 内存管理 | 节点动态分配（libstdc++ 使用 `std::allocator`） |
| 线程安全 | 本身非线程安全，需外部加锁；`std::map` 本身不加锁 |
| 异常安全 | 保证强异常安全（strong exception safety） |

### 与教学代码的差异

1. **模板化**：STL 版本是模板类，支持任意键值类型
2. **迭代器失效规则**：插入不导致迭代器失效，删除会导致指向被删节点的迭代器失效
3. **平衡因子维护**：实际工程中需额外维护父指针优化旋转
4. **哨兵节点优化**：部分实现使用单一哨兵节点（NIL）减少内存分配

### 使用示例

```cpp
#include <map>
std::map<int, std::string> m;
m[1] = "one";
m[2] = "two";
// 底层调用 _Rb_tree::insert_equal/insert_unique
```

### Linux 内核中的红黑树

Linux 内核使用自己的红黑树实现（`lib/rbtree.c` + `linux/rbtree.h`），用于进程调度、内存管理、文件系统等场景，与用户态实现的主要区别在于：
- 使用 `rb_node` 内联到宿主结构体中（而非独立节点）
- 无模板，纯 C 语言实现
- 针对性能做了 micro-optimization

## 参考

- 《算法导论》第 13 章：红黑树
- STL 源码：`libstdc++-v3/include/bits/stl_tree.h`
- Linux 内核：`lib/rbtree.c`, `linux/rbtree.h`
