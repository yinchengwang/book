/**
 * @file main.cpp
 * @brief C++ 自定义分配器演示
 *
 * 本文件演示：
 * 1. std::allocator 接口规范
 * 2. 简单内存池分配器实现
 * 3. Arena 分配器（预分配大块，按需切分）
 *
 * 学习目标：
 * - 理解 STL 分配器协议（allocate/deallocate/construct/destroy）
 * - 掌握内存池的原理（减少系统调用，提升分配效率）
 * - 理解 Arena 分配器的理念（批量分配、统一释放）
 */

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <list>
#include <new>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <limits>

// ================================================================
// 第一部分：标准分配器接口
// ================================================================

/**
 * @brief 最简分配器模板
 *
 * C++ 标准要求分配器提供以下接口：
 * - value_type: 管理的对象类型
 * - allocate(size_t n): 分配 n 个 value_type 的未构造内存
 * - deallocate(T* p, size_t n): 释放内存
 * - construct(T* p, Args...): 在已分配内存上构造对象（C++17 已废弃）
 * - destroy(T* p): 析构对象（C++17 已废弃）
 *
 * @note C++11 起建议使用 std::allocator_traits 代替直接调用 construct/destroy
 */
template <typename T>
class SimpleAllocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;

    // 默认构造函数
    SimpleAllocator() noexcept = default;

    // 拷贝构造函数（所有分配器实例互为兼容）
    template <typename U>
    SimpleAllocator(const SimpleAllocator<U>&) noexcept {}

    // 地址函数
    T* allocate(size_type n) {
        if (n == 0) return nullptr;
        if (n > max_size()) {
            throw std::bad_alloc();
        }
        void* ptr = std::malloc(n * sizeof(T));
        if (!ptr) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(ptr);
    }

    void deallocate(T* ptr, size_type) noexcept {
        std::free(ptr);
    }

    // 最大分配数量
    size_type max_size() const noexcept {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }

    // C++17 之前需要（现代 C++ 可忽略）
    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ::new (p) U(std::forward<Args>(args)...);
    }

    template <typename U>
    void destroy(U* p) {
        p->~U();
    }

    // 分配器相等性（C++11 起用于检测兼容性）
    template <typename U>
    bool operator==(const SimpleAllocator<U>&) const noexcept {
        return true;
    }

    template <typename U>
    bool operator!=(const SimpleAllocator<U>&) const noexcept {
        return false;
    }
};

// ================================================================
// 第二部分：内存池分配器
// ================================================================

/**
 * @brief 固定大小内存池
 *
 * 内存池的核心思想：
 * 1. 预先分配一大块内存（减少多次 malloc 的系统调用开销）
 * 2. 将大块切分为固定大小的 slot（块）
 * 3. 用空闲链表管理已释放的 slot（O(1) 分配/释放）
 *
 * 优点：
 * - 分配/释放时间复杂度 O(1)
 * - 避免内存碎片
 * - 减少系统调用次数
 *
 * 缺点：
 * - 只能分配固定大小的对象
 * - 预分配造成内存浪费
 */
template <typename T, std::size_t PoolSize = 1024>
class MemoryPool {
private:
    union Slot {
        char data[sizeof(T)];
        Slot* next;  // 空闲链表指针
    };

    Slot* free_list_;           // 空闲 slot 链表头
    char* memory_pool_;         // 预分配的内存池
    std::size_t pool_capacity_; // 内存池容量

public:
    using value_type = T;
    using size_type = std::size_t;
    using propagate_on_container_move_assignment = std::true_type;

    /**
     * @brief 构造函数：预分配整个内存池
     */
    MemoryPool() noexcept {
        pool_capacity_ = PoolSize;
        memory_pool_ = static_cast<char*>(std::malloc(pool_capacity_ * sizeof(Slot)));
        if (memory_pool_) {
            // 初始化空闲链表（将所有 slot 串起来）
            free_list_ = reinterpret_cast<Slot*>(memory_pool_);
            Slot* current = free_list_;
            for (std::size_t i = 1; i < pool_capacity_; ++i) {
                current->next = reinterpret_cast<Slot*>(memory_pool_ + i * sizeof(Slot));
                current = current->next;
            }
            current->next = nullptr;
        } else {
            free_list_ = nullptr;
        }
    }

    ~MemoryPool() {
        std::free(memory_pool_);
    }

    /**
     * @brief 从内存池分配一个 slot
     * @return 已分配 slot 的指针（未构造）
     */
    T* allocate() {
        if (!free_list_) {
            // 内存池耗尽，fallback 到系统分配
            void* ptr = std::malloc(sizeof(T));
            if (!ptr) throw std::bad_alloc();
            return static_cast<T*>(ptr);
        }

        Slot* slot = free_list_;
        free_list_ = free_list_->next;
        return reinterpret_cast<T*>(slot);
    }

    /**
     * @brief 释放一个 slot，回收到空闲链表
     */
    void deallocate(T* ptr) {
        if (!ptr) return;

        Slot* slot = reinterpret_cast<Slot*>(ptr);
        slot->next = free_list_;
        free_list_ = slot;
    }

    /**
     * @brief 构造对象
     */
    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ::new (p) U(std::forward<Args>(args)...);
    }

    /**
     * @brief 析构对象
     */
    template <typename U>
    void destroy(U* p) {
        p->~U();
    }

    std::size_t available() const {
        std::size_t count = 0;
        for (Slot* s = free_list_; s; s = s->next) ++count;
        return count;
    }
};

/**
 * @brief 基于内存池的分配器包装
 */
template <typename T, std::size_t PoolSize = 1024>
class PoolAllocator {
private:
    static MemoryPool<T, PoolSize>* pool_;

public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;

    // rebind 是 STL 容器要求的关键模板
    template <typename U>
    struct rebind {
        using other = PoolAllocator<U, PoolSize>;
    };

    PoolAllocator() noexcept = default;

    template <typename U>
    PoolAllocator(const PoolAllocator<U, PoolSize>&) noexcept {}

    T* allocate(size_type n) {
        if (n != 1) throw std::bad_alloc();
        T* ptr = pool_->allocate();
        if (!ptr) throw std::bad_alloc();
        return ptr;
    }

    void deallocate(T* ptr, size_type) noexcept {
        pool_->deallocate(ptr);
    }

    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        pool_->construct(p, std::forward<Args>(args)...);
    }

    template <typename U>
    void destroy(U* p) {
        pool_->destroy(p);
    }

    template <typename U>
    bool operator==(const PoolAllocator<U, PoolSize>&) const noexcept {
        return true;
    }

    template <typename U>
    bool operator!=(const PoolAllocator<U, PoolSize>&) const noexcept {
        return false;
    }

    static std::size_t available() { return pool_ ? pool_->available() : 0; }
};

// 静态成员定义
template <typename T, std::size_t PoolSize>
MemoryPool<T, PoolSize>* PoolAllocator<T, PoolSize>::pool_ = new MemoryPool<T, PoolSize>();

// ================================================================
// 第三部分：Arena 分配器
// ================================================================

/**
 * @brief 线性 Arena 分配器
 *
 * Arena（竞技场）分配器的特点：
 * 1. 预分配一大块连续内存
 * 2. 只支持"顺序分配"（指针递增），不支持单独释放
 * 3. 通过 reset() 一次性释放所有对象
 *
 * 适用场景：
 * - 临时对象的批量分配（如 AST 解析、帧缓冲）
 * - 分配模式明确、可预测的场景
 * - 需要极快分配速度的场景
 *
 * 优点：
 * - 分配速度极快（只需移动指针）
 * - 内存局部性好（缓存友好）
 *
 * 缺点：
 * - 无法释放单个对象
 * - 可能浪费未使用内存
 */
class LinearArena {
private:
    char* buffer_;          // 缓冲区起始地址
    char* current_;         // 当前分配位置
    char* end_;             // 缓冲区结束地址
    std::size_t total_;     // 缓冲区总大小

public:
    explicit LinearArena(std::size_t size) : total_(size) {
        buffer_ = static_cast<char*>(std::malloc(size));
        current_ = buffer_;
        end_ = buffer_ ? buffer_ + size : nullptr;
    }

    ~LinearArena() {
        std::free(buffer_);
    }

    // 禁用拷贝
    LinearArena(const LinearArena&) = delete;
    LinearArena& operator=(const LinearArena&) = delete;

    /**
     * @brief 分配内存
     * @param size 需要的字节数
     * @param alignment 对齐要求（必须是 2 的幂）
     * @return 分配到的内存地址
     */
    void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) {
        // 计算对齐后的地址
        std::uintptr_t ptr = reinterpret_cast<std::uintptr_t>(current_);
        std::uintptr_t aligned_ptr = (ptr + alignment - 1) & ~(alignment - 1);
        std::size_t padding = static_cast<std::size_t>(aligned_ptr - ptr);

        // 检查空间是否足够
        if (static_cast<std::size_t>(end_ - current_) < size + padding) {
            return nullptr;  // Arena 空间不足
        }

        current_ = reinterpret_cast<char*>(aligned_ptr + size);
        return reinterpret_cast<void*>(aligned_ptr);
    }

    /**
     * @brief 重置 Arena，释放所有已分配内存
     * @note O(1) 操作，不触发任何系统调用
     */
    void reset() noexcept {
        current_ = buffer_;
    }

    std::size_t used() const noexcept {
        return static_cast<std::size_t>(current_ - buffer_);
    }

    std::size_t capacity() const noexcept { return total_; }

    double utilization() const noexcept {
        return total_ > 0 ? 100.0 * used() / total_ : 0.0;
    }
};

// ================================================================
// 演示代码
// ================================================================

void print_separator(const char* title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void demo_std_allocator() {
    print_separator("1. std::allocator 使用演示");

    // 使用自定义 SimpleAllocator
    std::vector<int, SimpleAllocator<int>> vec;
    for (int i = 0; i < 10; ++i) {
        vec.push_back(i * i);
    }

    std::cout << "使用 SimpleAllocator<int> 的 vector: ";
    for (int v : vec) std::cout << v << " ";
    std::cout << "\n";

    // 使用自定义 SimpleAllocator 的 string
    std::vector<std::string, SimpleAllocator<std::string>> strings;
    strings.emplace_back("Hello");
    strings.emplace_back("Memory");
    strings.emplace_back("Allocator");

    std::cout << "使用 SimpleAllocator<string> 的 vector: ";
    for (const auto& s : strings) std::cout << s << " ";
    std::cout << "\n";
}

void demo_memory_pool() {
    print_separator("2. 内存池分配器演示");

    // 直接演示 MemoryPool 的使用
    MemoryPool<int, 128> pool;

    std::cout << "内存池初始可用 slot 数: " << pool.available() << "\n";

    // 手动分配和释放
    std::vector<int*> allocated;
    for (int i = 0; i < 50; ++i) {
        int* p = pool.allocate();
        pool.construct(p, i * i);
        allocated.push_back(p);
    }

    std::cout << "分配 50 个元素后可用 slot 数: " << pool.available() << "\n";

    // 释放一半
    for (int i = 0; i < 25; ++i) {
        pool.destroy(allocated[i]);
        pool.deallocate(allocated[i]);
    }

    std::cout << "释放 25 个元素后可用 slot 数: " << pool.available() << "\n";

    std::cout << "剩余元素值: ";
    for (int i = 25; i < 50; ++i) {
        std::cout << *allocated[i] << " ";
    }
    std::cout << "\n";

    // 清理
    for (int i = 25; i < 50; ++i) {
        pool.destroy(allocated[i]);
        pool.deallocate(allocated[i]);
    }

    std::cout << "最终可用 slot 数: " << pool.available() << "\n";
}

void demo_arena() {
    print_separator("3. Arena 分配器演示");

    LinearArena arena(1024);  // 1KB arena

    std::cout << "Arena 容量: " << arena.capacity() << " bytes\n";

    // 模拟分配多个临时对象
    struct TempObj {
        int id;
        double data[4];
    };

    std::vector<TempObj*> objects;

    // 分配 10 个对象
    for (int i = 0; i < 10; ++i) {
        TempObj* obj = static_cast<TempObj*>(arena.allocate(sizeof(TempObj), alignof(TempObj)));
        if (obj) {
            obj->id = i;
            for (int j = 0; j < 4; ++j) obj->data[j] = i * 0.1 + j;
            objects.push_back(obj);
        }
    }

    std::cout << "分配 10 个 TempObj 后:\n";
    std::cout << "  - 已使用: " << arena.used() << " bytes\n";
    std::cout << "  - 利用率: " << arena.utilization() << "%\n";

    // 使用 arena 中的对象
    std::cout << "对象数据: ";
    for (auto* obj : objects) {
        std::cout << "(" << obj->id << "," << obj->data[0] << ") ";
    }
    std::cout << "\n";

    // 重置 arena（所有对象被"释放"）
    arena.reset();
    std::cout << "调用 reset() 后:\n";
    std::cout << "  - 已使用: " << arena.used() << " bytes\n";
    std::cout << "  - 利用率: " << arena.utilization() << "%\n";

    // 再次分配（新对象覆盖旧数据）
    auto* new_obj = static_cast<int*>(arena.allocate(sizeof(int)));
    *new_obj = 42;
    std::cout << "reset 后重新分配: *new_obj = " << *new_obj << "\n";
}

int main() {
    std::cout << "C++ 自定义分配器演示\n";
    std::cout << "====================\n";

    demo_std_allocator();
    demo_memory_pool();
    demo_arena();

    print_separator("演示完成");
    std::cout << "本演示展示了三种内存分配策略：\n";
    std::cout << "1. std::allocator: 标准分配器接口\n";
    std::cout << "2. 内存池: 固定大小块的复用池\n";
    std::cout << "3. Arena: 线性分配，批量释放\n";
    std::cout << "\n";

    return 0;
}
