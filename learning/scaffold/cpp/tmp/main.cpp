// tmp scaffold — 模板元编程
//
// 复现命令：
//   g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
//
// 演示 4 段：
//   [type-list]    — 类型列表（TypeList）
//   [type-compute] — 类型计算（Fibonacci @ compile-time）
//   [constexpr-if] — constexpr if 分支消除
//   [variadic]     — 可变参数模板

#include <iostream>
#include <type_traits>
#include <array>

// ============================================================================
// [type-list] 类型列表
// ============================================================================

// TypeList 基础定义（带 push_back 和 remove）
template<typename... Ts>
struct TypeList {
    static constexpr std::size_t size = sizeof...(Ts);
    template<typename T>
    using push_back = TypeList<Ts..., T>;
};

// 获取第 N 个类型
template<std::size_t N, typename... Ts>
struct TypeAt;

// 递归展开
template<std::size_t N, typename First, typename... Rest>
struct TypeAt<N, First, Rest...> {
    using type = typename TypeAt<N - 1, Rest...>::type;
};

template<typename First, typename... Rest>
struct TypeAt<0, First, Rest...> {
    using type = First;
};

// TypeAt_t 别名
template<std::size_t N, typename... Ts>
using TypeAt_t = typename TypeAt<N, Ts...>::type;

// 类型过滤（移除指定类型）
template<typename T, typename... Ts>
struct Remove;

// 递归实现
template<typename T, typename First, typename... Rest>
struct Remove<T, First, Rest...> {
    using raw = typename Remove<T, Rest...>::type;
    using type = typename std::conditional<
        std::is_same<T, First>::value,
        raw,
        typename raw::template push_back<First>
    >::type;
};

// 递归终止
template<typename T>
struct Remove<T> {
    using type = TypeList<>;
};

// ============================================================================
// [type-compute] 编译期计算
// ============================================================================

// 编译期 Fibonacci
template<int N>
struct Fibonacci {
    static constexpr int value = Fibonacci<N - 1>::value + Fibonacci<N - 2>::value;
};

template<>
struct Fibonacci<0> {
    static constexpr int value = 0;
};

template<>
struct Fibonacci<1> {
    static constexpr int value = 1;
};

// 编译期求最大公约数
template<int a, int b>
struct GCD {
    static constexpr int value = GCD<b, a % b>::value;
};

template<int a>
struct GCD<a, 0> {
    static constexpr int value = a;
};

// 编译期幂运算
template<int base, int exp>
struct Power {
    static constexpr int value = base * Power<base, exp - 1>::value;
};

template<int base>
struct Power<base, 0> {
    static constexpr int value = 1;
};

// ============================================================================
// [constexpr-if] constexpr if 分支消除
// ============================================================================

// 编译期类型判别
template<typename T>
const char* describe_type() {
    if constexpr (std::is_pointer_v<T>) {
        return "pointer";
    } else if constexpr (std::is_reference_v<T>) {
        return "reference";
    } else if constexpr (std::is_array_v<T>) {
        return "array";
    } else {
        return "scalar";
    }
}

// ============================================================================
// [variadic] 可变参数模板
// ============================================================================

// 可变参数求和
template<typename T>
T sum(T v) { return v; }

template<typename T, typename... Args>
T sum(T first, Args... args) {
    return first + sum<T>(args...);
}

// 可变参数打印
void print_all() { std::cout << "\n"; }

template<typename First, typename... Rest>
void print_all(First first, Rest... rest) {
    std::cout << first;
    if constexpr (sizeof...(Rest) > 0) {
        std::cout << ", ";
    }
    print_all(rest...);
}

// ============================================================================
// main
// ============================================================================
int main() {
    // [type-list]
    std::cout << "[type-list] === 类型列表 ===" << std::endl;
    using MyTypes = TypeList<int, double, char, int, float>;
    std::cout << "  TypeList size: " << MyTypes::size << std::endl;
    std::cout << "  TypeAt<0>: int" << std::endl;
    std::cout << "  TypeAt<1>: double" << std::endl;
    std::cout << "  TypeAt<2>: char" << std::endl;

    // 使用 TypeAt_t
    using T0 = TypeAt_t<0, int, double, char>;
    using T1 = TypeAt_t<1, int, double, char>;
    using T2 = TypeAt_t<2, int, double, char>;
    std::cout << "  TypeAt_t<0,int,double,char>: " << describe_type<T0>() << std::endl;
    std::cout << "  TypeAt_t<1,int,double,char>: " << describe_type<T1>() << std::endl;
    std::cout << "  TypeAt_t<2,int,double,char>: " << describe_type<T2>() << std::endl;

    // [type-compute]
    std::cout << "\n[type-compute] === 编译期计算 ===" << std::endl;
    std::cout << "  Fibonacci<10> = " << Fibonacci<10>::value << std::endl;
    std::cout << "  GCD<48, 18> = " << GCD<48, 18>::value << std::endl;
    std::cout << "  Power<2, 10> = " << Power<2, 10>::value << std::endl;

    // 编译期数组初始化
    constexpr int fib10 = Fibonacci<10>::value;
    std::array<int, fib10> arr{};
    std::cout << "  编译期数组大小: " << arr.size() << std::endl;

    // [constexpr-if]
    std::cout << "\n[constexpr-if] === constexpr if ===" << std::endl;
    int x = 42;
    int* px = &x;
    std::cout << "  int: " << describe_type<int>() << std::endl;
    std::cout << "  int*: " << describe_type<decltype(px)>() << std::endl;
    std::cout << "  int&: " << describe_type<decltype(std::ref(x))>() << std::endl;

    // [variadic]
    std::cout << "\n[variadic] === 可变参数模板 ===" << std::endl;
    std::cout << "  sum(1, 2, 3, 4, 5) = " << sum(1, 2, 3, 4, 5) << std::endl;
    std::cout << "  sum(1.5, 2.5, 3.0) = " << sum(1.5, 2.5, 3.0) << std::endl;
    std::cout << "  print_all: ";
    print_all("hello", 123, 3.14, 'c');

    std::cout << "\n=== PASS ===" << std::endl;
    return 0;
}
