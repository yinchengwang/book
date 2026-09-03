// sfinae scaffold — SFINAE 与 Concepts
//
// 复现命令：
//   g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
//
// 演示 4 段：
//   [sfinae]       — SFINAE 原则与 enable_if
//   [void-t]       — void_t 技巧
//   [concept]      — C++20 Concepts（C++17 环境演示）
//   [requires]     — requires 表达式

#include <iostream>
#include <type_traits>
#include <vector>
#include <list>
#include <set>

// ============================================================================
// [sfinae] SFINAE 原则
// ============================================================================

// SFINAE: Substitution Failure Is Not An Error
// 当模板参数替换失败时，编译器选择其他重载而非报错

// 检测是否有 size_type 成员
template<typename T, typename = void>
struct has_size_type : std::false_type {};

template<typename T>
struct has_size_type<T, std::void_t<typename T::size_type>> : std::true_type {};

// 检测是否有 value_type 成员
template<typename T, typename = void>
struct has_value_type : std::false_type {};

template<typename T>
struct has_value_type<T, std::void_t<typename T::value_type>> : std::true_type {};

// 使用 enable_if 限制重载
template<typename T>
std::enable_if_t<has_size_type<T>::value, std::size_t>
get_size(const T& container) {
    return container.size();
}

template<typename T>
std::enable_if_t<!has_size_type<T>::value, std::size_t>
get_size(const T&) {
    return 0;  // 无 size 方法返回 0
}

// ============================================================================
// [void-t] void_t 技巧
// ============================================================================

// void_t 将任意类型映射为 void
// 常用于检测表达式有效性

// 检测是否可以调用 begin()
template<typename T, typename = void>
struct has_begin : std::false_type {};

template<typename T>
struct has_begin<T, std::void_t<decltype(std::declval<T>().begin())>> : std::true_type {};

// 检测是否可以调用 + 运算符
template<typename T, typename = void>
struct has_plus : std::false_type {};

template<typename T>
struct has_plus<T, std::void_t<decltype(std::declval<T>() + std::declval<T>())>> : std::true_type {};

// ============================================================================
// [concept] Concepts（C++17 演示概念检查方式）
// ============================================================================

// C++17 用 constexpr 函数模拟 Concepts
template<typename T>
constexpr bool is_container_v = has_begin<T>::value && has_size_type<T>::value;

template<typename T>
constexpr bool is_arithmetic_v = std::is_arithmetic_v<T>;

// Container 概念（C++17 模拟）
template<typename T>
auto container_info(const T& c)
    -> std::enable_if_t<is_container_v<T>, std::string>
{
    return "container with size " + std::to_string(c.size());
}

template<typename T>
auto container_info(const T&)
    -> std::enable_if_t<!is_container_v<T>, std::string>
{
    return "not a container";
}

// ============================================================================
// [requires] C++20 requires 表达式（C++17 用 if constexpr 替代）
// ============================================================================

// 安全的类型打印
template<typename T>
void describe(const T& val) {
    if constexpr (std::is_integral_v<T>) {
        std::cout << "integral: " << val << "\n";
    } else if constexpr (std::is_floating_point_v<T>) {
        std::cout << "floating: " << val << "\n";
    } else if constexpr (is_container_v<T>) {
        std::cout << "container: size=" << val.size() << "\n";
    } else {
        std::cout << "other type\n";
    }
}

// ============================================================================
// main
// ============================================================================
int main() {
    // [sfinae]
    std::cout << "[sfinae] === SFINAE 原则 ===" << std::endl;
    std::vector<int> v = {1, 2, 3};
    std::list<int> l = {4, 5, 6};
    int arr[] = {7, 8, 9};

    std::cout << "  vector size: " << get_size(v) << std::endl;
    std::cout << "  list size: " << get_size(l) << std::endl;
    std::cout << "  array size: " << get_size(arr) << std::endl;

    // [void-t]
    std::cout << "\n[void-t] === void_t 技巧 ===" << std::endl;
    std::cout << "  has_begin<vector>: " << has_begin<std::vector<int>>::value << std::endl;
    std::cout << "  has_begin<int>: " << has_begin<int>::value << std::endl;
    std::cout << "  has_plus<int>: " << has_plus<int>::value << std::endl;

    // [concept]
    std::cout << "\n[concept] === Concepts ===" << std::endl;
    std::cout << "  is_container<vector>: " << is_container_v<std::vector<int>> << std::endl;
    std::cout << "  is_container<int>: " << is_container_v<int> << std::endl;
    std::cout << "  is_arithmetic<int>: " << is_arithmetic_v<int> << std::endl;
    std::cout << "  is_arithmetic<double>: " << is_arithmetic_v<double> << std::endl;
    std::cout << "  " << container_info(v) << std::endl;
    std::cout << "  " << container_info(arr) << std::endl;

    // [requires]
    std::cout << "\n[requires] === 类型检测 ===" << std::endl;
    describe(42);
    describe(3.14);
    describe(v);

    std::cout << "\n=== PASS ===" << std::endl;
    return 0;
}
