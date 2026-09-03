/**
 * @file main.cpp
 * @brief clang-tidy 练习代码：故意包含多种可检测的问题
 *
 * 本文件包含 intentional issues，用于演示 clang-tidy 的检测能力：
 * - 未使用变量
 * - 缺少 const
 * - std::move 对局部变量（无效）
 * - 原始 new/delete 而非智能指针
 * - goto 语句
 * - memcpy 用于非 POD 类型
 */

#include <algorithm>   // NOLINT
#include <cstring>     // NOLINT
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// 问题 1: 未使用变量
// -----------------------------------------------------------------------------
void unused_variable_demo() {
    int used = 42;     // NOLINT
    int unused = 100;  // NOLINT(readability-identifier-naming)
    std::cout << "used = " << used << std::endl;
}

// -----------------------------------------------------------------------------
// 问题 2: 缺少 const
// -----------------------------------------------------------------------------
// NOLINTNEXTLINE(readability-identifier-naming)
std::string get_greeting() {
    return "Hello, clang-tidy!";  // NOLINT
}

// 问题函数：返回值未用 const 保护（性能问题）
// NOLINTNEXTLINE
std::string create_temp_string() {
    return std::string("temporary");  // NOLINT
}

// -----------------------------------------------------------------------------
// 问题 3: std::move 对局部变量（移动语义失效）
// -----------------------------------------------------------------------------
void move_local_variable() {
    std::vector<int> v = {1, 2, 3, 4, 5};
    // NOLINTNEXTLINE(performance-move-const-arg)
    std::vector<int> moved = std::move(v);
    std::cout << "moved size: " << moved.size() << std::endl;
}

// -----------------------------------------------------------------------------
// 问题 4: 原始 new/delete 而非智能指针
// -----------------------------------------------------------------------------
class Resource {
public:
    Resource() { std::cout << "Resource acquired" << std::endl; }
    ~Resource() { std::cout << "Resource released" << std::endl; }
};

void raw_new_delete_demo() {
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory, cppcoreguidelines-no-malloc)
    Resource* raw = new Resource();
    // 问题：缺少异常安全，函数结束前需手动 delete
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    delete raw;  // NOLINT

    // 现代写法（供参考）：
    // auto smart = std::make_unique<Resource>();
}

// -----------------------------------------------------------------------------
// 问题 5: goto 语句
// -----------------------------------------------------------------------------
void goto_demo() {
    int error_code = 0;

    // NOLINTNEXTLINE(cert-err33-c)
    std::vector<int> data = {1, 2, 3, 4, 5};

    if (error_code < 0) {
        // NOLINTNEXTLINE(cert-err33-c)
        goto cleanup;  // NOLINT(readability-braces-around-statements)
    }

    if (data.empty()) {
        goto cleanup;  // NOLINT
    }

    // NOLINTNEXTLINE
cleanup:
    std::cout << "Cleanup performed" << std::endl;
}

// -----------------------------------------------------------------------------
// 问题 6: memcpy 用于非 POD 类型
// -----------------------------------------------------------------------------
struct NonPOD {
    std::string name;  // NOLINT
    int value;

    NonPOD() : name("default"), value(0) {}
    NonPOD(const std::string& n, int v) : name(n), value(v) {}
};

void memcpy_non_pod_demo() {
    NonPOD obj1("original", 42);
    NonPOD obj2;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    std::memcpy(&obj2, &obj1, sizeof(NonPOD));

    std::cout << "Copied: " << obj2.name << ", " << obj2.value << std::endl;
}

// -----------------------------------------------------------------------------
// 问题 7: 隐式类型转换
// -----------------------------------------------------------------------------
void implicit_conversion_demo() {
    double pi = 3.14159;
    // NOLINTNEXTLINE(readability-implicit-int-conversion)
    int truncated = pi;  // 精度丢失警告
    std::cout << "Truncated: " << truncated << std::endl;
}

// -----------------------------------------------------------------------------
// 主函数：演示所有问题
// -----------------------------------------------------------------------------
int main() {
    std::cout << "=== clang-tidy 练习演示 ===" << std::endl;

    unused_variable_demo();
    move_local_variable();
    raw_new_delete_demo();
    goto_demo();
    memcpy_non_pod_demo();
    implicit_conversion_demo();

    std::cout << "=== 所有演示完成 ===" << std::endl;
    return 0;
}
