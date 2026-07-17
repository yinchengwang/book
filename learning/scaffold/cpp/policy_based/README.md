# C++ Policy 模式

> Policy 类模板 + 组合策略 + 编译期多态

## 编译

```bash
g++ -std=c++17 -o main main.cpp
./main
```

## 什么是 Policy 模式

Policy 模式通过模板参数组合不同的策略：

```cpp
template<typename ThreadModel, typename AllocModel>
class SmartPtr {
    // 使用 ThreadModel 和 AllocModel 作为 Policy
};
```

## Policy 组合

| Policy 类型 | 示例 |
|-------------|------|
| 线程安全 | SingleThreaded / MultiThreaded |
| 内存分配 | HeapAllocation / PoolAllocation |
| 序列化 | JsonSerializer / XmlSerializer |
| 存储后端 | MemoryStorage / FileStorage |

## 运行结果

```
=== Policy 模式演示 ===

1. Logging Policy:
[LOG] This is a log message

2. Thread Safety Policy:
Single-threaded operation

3. SmartPtr with Policies:
Value: 42
Thread-safe operation

4. Storage with Policies:
JSON: "world"
XML: <data>hello</data>

5. Compile-Time Policy Selection:
Cache enabled: Cached
Cache disabled: NoCache

6. STL Policy Examples:
Sorted (less): 1 1 3 4 5
Sorted (greater): 5 4 3 1 1

7. Allocator Policy:
Heap allocation: new/delete
Pool allocation: pre-allocated buffer
```
