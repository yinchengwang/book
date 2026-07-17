/**
 * @file main.cpp
 * @brief C++ 移动语义演示 - 深入理解右值引用、移动构造、移动赋值和 std::move
 *
 * 本文件演示 C++11 引入的移动语义，包括：
 * - 左值与右值的区别
 * - 右值引用的声明与使用
 * - 移动构造函数与移动赋值运算符
 * - std::move 的原理与使用场景
 * - 深拷贝 vs 移动的性能差异对比
 */

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <cassert>

// ============================================================================
// 第一部分：自定义类演示移动语义
// ============================================================================

/**
 * @brief 演示类：包含大型资源，用于对比深拷贝与移动的性能
 *
 * 该类模拟一个管理动态内存的资源类：
 * - 构造函数：分配内存并填充数据
 * - 拷贝构造函数：深拷贝，复制所有数据
 * - 移动构造函数：转移资源所有权，原对象置空
 * - 析构函数：释放内存
 */
class BigResource {
private:
    int* data_;        // 指向动态分配内存的指针
    size_t size_;      // 内存块大小

public:
    /**
     * @brief 默认构造函数
     */
    BigResource() : data_(nullptr), size_(0) {
        std::cout << "  [BigResource] 默认构造: 空对象\n";
    }

    /**
     * @brief 参数化构造函数：分配指定大小的内存
     * @param size 内存块大小（单位：整数个数）
     */
    explicit BigResource(size_t size) : size_(size) {
        data_ = new int[size];
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = static_cast<int>(i);
        }
        std::cout << "  [BigResource] 构造: 分配 " << size_ << " 个整数\n";
    }

    /**
     * @brief 拷贝构造函数：执行深拷贝
     *
     * 注意：这个操作会分配新的内存并复制所有数据，
     * 对于大型对象来说开销很大。
     */
    BigResource(const BigResource& other) : size_(other.size_) {
        data_ = new int[size_];
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
        std::cout << "  [BigResource] 拷贝构造: 深拷贝 " << size_ << " 个整数\n";
    }

    /**
     * @brief 移动构造函数：转移资源所有权
     *
     * 注意：这个操作只是"偷取"原对象的资源，将原对象的指针置为 nullptr。
     * 不会分配新内存，也不会复制数据，因此效率极高。
     */
    BigResource(BigResource&& other) noexcept : data_(other.data_), size_(other.size_) {
        // 重要：将源对象的资源置空，避免析构函数重复释放内存
        other.data_ = nullptr;
        other.size_ = 0;
        std::cout << "  [BigResource] 移动构造: 转移资源所有权\n";
    }

    /**
     * @brief 拷贝赋值运算符：执行深拷贝
     */
    BigResource& operator=(const BigResource& other) {
        if (this != &other) {
            // 先释放原有资源
            delete[] data_;
            // 再分配新内存并复制
            size_ = other.size_;
            data_ = new int[size_];
            for (size_t i = 0; i < size_; ++i) {
                data_[i] = other.data_[i];
            }
            std::cout << "  [BigResource] 拷贝赋值: 深拷贝 " << size_ << " 个整数\n";
        }
        return *this;
    }

    /**
     * @brief 移动赋值运算符：转移资源所有权
     */
    BigResource& operator=(BigResource&& other) noexcept {
        if (this != &other) {
            // 先释放原有资源
            delete[] data_;
            // 转移资源所有权
            data_ = other.data_;
            size_ = other.size_;
            // 将源对象置空
            other.data_ = nullptr;
            other.size_ = 0;
            std::cout << "  [BigResource] 移动赋值: 转移资源所有权\n";
        }
        return *this;
    }

    /**
     * @brief 析构函数：释放内存
     */
    ~BigResource() {
        if (data_ != nullptr) {
            std::cout << "  [BigResource] 析构: 释放 " << size_ << " 个整数\n";
            delete[] data_;
        } else {
            std::cout << "  [BigResource] 析构: 空对象\n";
        }
    }

    /**
     * @brief 获取资源大小
     */
    size_t size() const { return size_; }

    /**
     * @brief 检查资源是否有效
     */
    bool isValid() const { return data_ != nullptr; }
};

// ============================================================================
// 第二部分：工厂函数演示返回值的移动优化
// ============================================================================

/**
 * @brief 创建 BigResource 对象（演示返回值优化）
 *
 * 在 C++11 及以后，返回局部对象的移动语义是自动的，
 * 编译器会尝试使用移动构造而不是拷贝构造。
 */
BigResource createResource(size_t size) {
    BigResource resource(size);
    return resource;  // 这里会发生移动构造（如果有的话）
}

// ============================================================================
// 第三部分：演示 std::move 的使用
// ============================================================================

/**
 * @brief 演示 std::move 将左值转换为右值引用
 *
 * std::move 本身不移动任何东西，它只是一个类型转换：
 * - 将 T& 转换为 T&&（右值引用）
 * - 将 const T& 转换为 const T&&
 *
 * 转换后，对象可以"被移动"。
 */
void processResource(BigResource&& resource) {
    std::cout << "  处理资源，大小: " << resource.size() << "\n";
}

// ============================================================================
// 主函数：运行所有演示
// ============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "C++ 移动语义演示\n";
    std::cout << "========================================\n\n";

    // ------------------------------------------------------------------
    // 演示 1：基本的移动构造
    // ------------------------------------------------------------------
    std::cout << "【演示 1】移动构造\n";
    std::cout << "----------------------------------------\n";
    {
        BigResource r1(100);           // 构造 r1
        std::cout << "  将 r1 移动构造 r2...\n";
        BigResource r2 = std::move(r1); // 移动构造 r2

        // 验证移动结果
        assert(!r1.isValid());          // r1 应该为空
        assert(r2.isValid());           // r2 应该持有资源
        assert(r1.size() == 0);         // r1 的大小应为 0
        assert(r2.size() == 100);       // r2 的大小应为 100
    }
    std::cout << "\n";

    // ------------------------------------------------------------------
    // 演示 2：移动赋值
    // ------------------------------------------------------------------
    std::cout << "【演示 2】移动赋值\n";
    std::cout << "----------------------------------------\n";
    {
        BigResource r1(200);
        BigResource r2(50);
        std::cout << "  将 r1 移动赋值给 r2...\n";
        r2 = std::move(r1);

        assert(!r1.isValid());
        assert(r2.size() == 200);
    }
    std::cout << "\n";

    // ------------------------------------------------------------------
    // 演示 3：深拷贝 vs 移动的性能对比
    // ------------------------------------------------------------------
    std::cout << "【演示 3】深拷贝 vs 移动性能对比\n";
    std::cout << "----------------------------------------\n";

    const size_t N = 1000000;  // 每个对象 100 万个整数
    const int LOOP = 10;       // 重复 10 次取平均

    // 测试深拷贝性能
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < LOOP; ++i) {
        BigResource src(N);
        BigResource dst(src);  // 拷贝构造（深拷贝）
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto copy_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  深拷贝耗时: " << copy_duration.count() << " ms\n";

    // 测试移动性能
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < LOOP; ++i) {
        BigResource src(N);
        BigResource dst(std::move(src));  // 移动构造（零拷贝）
    }
    end = std::chrono::high_resolution_clock::now();
    auto move_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  移动耗时:   " << move_duration.count() << " ms\n";

    // 输出对比
    if (move_duration.count() > 0) {
        double ratio = static_cast<double>(copy_duration.count()) / move_duration.count();
        std::cout << "  移动比深拷贝快约 " << ratio << " 倍\n";
    } else {
        std::cout << "  移动操作极快（毫秒级以下）\n";
    }
    std::cout << "\n";

    // ------------------------------------------------------------------
    // 演示 4：std::move 在容器操作中的应用
    // ------------------------------------------------------------------
    std::cout << "【演示 4】std::move 在 vector 操作中的应用\n";
    std::cout << "----------------------------------------\n";
    {
        std::vector<BigResource> vec;
        vec.reserve(3);  // 预分配空间，避免重新分配

        std::cout << "  插入第一个对象...\n";
        vec.push_back(BigResource(10));

        std::cout << "  使用 std::move 插入第二个对象...\n";
        BigResource r(20);
        vec.push_back(std::move(r));

        std::cout << "  vector 现在有 " << vec.size() << " 个元素\n";
    }
    std::cout << "\n";

    // ------------------------------------------------------------------
    // 演示 5：unique_ptr 的移动语义
    // ------------------------------------------------------------------
    std::cout << "【演示 5】unique_ptr 的移动语义\n";
    std::cout << "----------------------------------------\n";
    {
        // unique_ptr 不能拷贝，只能移动
        std::unique_ptr<int> ptr1 = std::make_unique<int>(42);
        std::cout << "  ptr1 指向: " << *ptr1 << "\n";

        std::cout << "  移动 ptr1 到 ptr2...\n";
        std::unique_ptr<int> ptr2 = std::move(ptr1);

        // 验证移动结果
        assert(ptr1 == nullptr);
        assert(ptr2 != nullptr);
        assert(*ptr2 == 42);

        std::cout << "  ptr1 为空: " << (ptr1 == nullptr ? "是" : "否") << "\n";
        std::cout << "  ptr2 指向: " << *ptr2 << "\n";
    }
    std::cout << "\n";

    std::cout << "========================================\n";
    std::cout << "演示完成！\n";
    std::cout << "========================================\n";

    return 0;
}
