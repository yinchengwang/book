# C++ 自定义分配器

## 概念

**自定义分配器（Custom Allocator）** 是 C++ STL 中管理内存分配和释放的策略对象。它是 STL 容器与底层内存之间的抽象层，允许开发者替换默认的 `std::allocator` 以实现特定的内存管理策略。

## 标准分配器接口

C++ 标准要求分配器提供以下核心接口：

| 接口 | 说明 |
|------|------|
| `value_type` | 管理的对象类型 |
| `allocate(n)` | 分配 n 个对象的未构造内存 |
| `deallocate(p, n)` | 释放从 allocate 获得的内存 |
| `construct(p, args...)` | 在已分配内存上构造对象（C++17 前） |
| `destroy(p)` | 析构对象（C++17 前） |

## 常见分配器类型

### 1. std::allocator

标准分配器，使用 `new`/`delete`，适合通用场景。

```cpp
std::vector<int, std::allocator<int>> v;
```

### 2. 内存池分配器

预先分配一大块内存，切分为固定大小的 slot，通过空闲链表管理分配和释放。

- **优点**: O(1) 分配/释放，避免碎片
- **缺点**: 只适合固定大小的对象

### 3. Arena 分配器

预分配连续内存，只支持顺序分配，通过一次性 reset 释放所有内存。

- **优点**: 分配极快，缓存友好
- **缺点**: 无法单独释放对象

### 4. 对象池（Object Pool）

预先创建一组对象实例，复用而非销毁重建。

- **适用**: 频繁创建/销毁同类对象的场景（如游戏中的子弹、粒子）

## 选择建议

| 场景 | 推荐分配器 |
|------|-----------|
| 通用容器 | std::allocator |
| 高频小对象分配 | 内存池分配器 |
| 临时对象批量处理 | Arena 分配器 |
| 连接池、线程池 | 对象池 |
| 嵌入式/实时系统 | 内存池 / Arena |

## 参考资料

- [C++ Standard - Allocators](https://en.cppreference.com/w/cpp/memory/allocator)
- [Memory Pool Design Patterns](https://www.memorymanagement.org/)
- 《Effective STL》Item 10-13: 分配器相关最佳实践
