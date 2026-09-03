# 移动语义 (Move Semantics)

## 概念介绍

移动语义是 C++11 引入的核心特性，它解决了 C++ 中资源所有权的转移问题，避免了不必要的深拷贝操作。

### 左值与右值

- **左值 (lvalue)**：可以取地址、有名字的表达式，如变量
- **右值 (rvalue)**：临时的、即将销毁的表达式，如字面量、临时对象

```cpp
int a = 5;      // 5 是右值，a 是左值
int b = a + 1;  // a+1 是右值
```

### 右值引用 (T&&)

右值引用绑定到右值，实现"窃取"右值资源的能力：

```cpp
void process(int&& rref) {  // 接收临时对象
    // rref 可以安全使用，因为原对象即将销毁
}
process(5);  // 字面量 5 是右值
```

### 移动构造与移动赋值

```cpp
class Buffer {
    char* data_;
    size_t size_;
public:
    // 移动构造函数
    Buffer(Buffer&& other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;  // 关键：阻止析构函数释放
        other.size_ = 0;
    }

    // 移动赋值运算符
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }
};
```

### std::move

`std::move` 本质是类型转换，将左值转为右值引用：

```cpp
T a;
T b = std::move(a);  // 调用移动构造，而非拷贝构造
```

### 性能对比

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| 深拷贝 | O(n) | 分配内存，复制所有数据 |
| 移动 | O(1) | 指针交换，原对象置空 |

### 常见应用场景

1. **容器操作**：`vector::push_back` 会移动而非拷贝大对象
2. **智能指针**：`unique_ptr` 只能移动不能拷贝
3. **工厂函数**：返回局部对象时触发移动构造
4. **资源管理**：RAII 类的所有权转移

### noexcept 的重要性

移动构造函数和移动赋值运算符应该标记为 `noexcept`，否则：
- `vector` 扩容时可能选择拷贝而非移动
- 标准库容器对移动语义的优化会失效
