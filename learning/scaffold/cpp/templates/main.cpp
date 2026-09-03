/**
 * @file main.cpp
 * @brief C++ 模板元编程演示
 *
 * 本文件演示 C++ 模板的核心特性：
 * - 函数模板（Function Template）
 * - 类模板（Class Template）
 * - 模板特化（Template Specialization）
 * - 可变参数模板（Variadic Template）
 * - SFINAE 思想（Substitution Failure Is Not An Error）
 * - constexpr if（C++17）
 *
 * 编译: g++ -std=c++17 -Wall -Wextra -g -o test main.cpp
 * 运行: ./test
 */

#include <iostream>
#include <vector>
#include <type_traits>
#include <string>
#include <cstring>

// ============================================================
// 第一部分：函数模板基础
// ============================================================

/**
 * @brief 通用最大值函数模板
 *
 * 使用模板实现泛型最大值计算，适用于任何支持比较运算的类型。
 * 编译器在编译时进行参数替换，生成类型安全的代码。
 *
 * @tparam T 类型参数，由编译器自动推导
 * @param a 第一个值
 * @param b 第二个值
 * @return 较大值
 */
template <typename T>
T max_value(T a, T b) {
    return (a > b) ? a : b;
}

/**
 * @brief 类型无关的模板重载
 *
 * 针对于 const char* 的特化版本，
 * 演示如何为特定类型提供不同的实现。
 */
template <>
const char* max_value<const char*>(const char* a, const char* b) {
    return (std::strcmp(a, b) > 0) ? a : b;
}

// ============================================================
// 第二部分：类模板
// ============================================================

/**
 * @brief 简单的堆栈类模板
 *
 * 使用类模板实现泛型栈，支持任意类型的 push/pop 操作。
 * 模板参数 T 指定栈中元素的类型，模板参数 N 指定栈的容量。
 *
 * @tparam T 元素类型
 * @tparam N 栈容量，默认 100
 */
template <typename T, size_t N = 100>
class Stack {
private:
    T data_[N];       // 底层存储
    size_t top_;      // 栈顶指针

public:
    // 构造函数：初始化空栈
    Stack() : top_(0) {}

    // 入栈操作
    void push(const T& value) {
        if (top_ >= N) {
            throw std::overflow_error("Stack overflow");
        }
        data_[top_++] = value;
    }

    // 出栈操作
    T pop() {
        if (top_ == 0) {
            throw std::underflow_error("Stack underflow");
        }
        return data_[--top_];
    }

    // 获取栈顶元素（不弹出）
    const T& top() const {
        if (top_ == 0) {
            throw std::underflow_error("Stack is empty");
        }
        return data_[top_ - 1];
    }

    // 判断栈是否为空
    bool empty() const { return top_ == 0; }

    // 获取栈大小
    size_t size() const { return top_; }
};

// ============================================================
// 第三部分：SFINAE 与类型 traits
// ============================================================

/**
 * @brief SFINAE：检测类型是否支持加法运算
 *
 * SFINAE 原理：当模板参数替换失败时，编译器不会报错，
 * 而是简单地跳过这个模板候选，转而尝试其他重载。
 *
 * enable_if_t<condition, T> 在 condition 为 false 时
 * 会导致替换失败，从而使得该函数模板被排除。
 *
 * @tparam T 类型参数
 * @param a 第一个值
 * @param b 第二个值
 */
template <typename T>
auto add(T a, T b) -> typename std::enable_if_t<
    std::is_arithmetic_v<T>, T> {
    return a + b;
}

// 字符串特化版本（不支持加法的类型走这里）
template <typename T>
std::string add(const T& a, const T& b) {
    return std::to_string(a) + " + " + std::to_string(b);
}

/**
 * @brief 检测类型是否可迭代的 trait
 *
 * 通过 SFINAE 检测类型是否定义了 begin() 和 end() 方法。
 * 使用 void_t 技巧来实现这个检测。
 */
template <typename T, typename = void>
struct is_iterable : std::false_type {};

// 特化版本：仅当 T 有 begin() 方法时匹配
template <typename T>
struct is_iterable<T, std::void_t<decltype(std::declval<T>().begin())>>
    : std::true_type {};

// C++14 及以后可用的 _v 变量模板（简化 is_iterable::value 的使用）
template <typename T>
inline constexpr bool is_iterable_v = is_iterable<T>::value;

// ============================================================
// 第四部分：可变参数模板
// ============================================================

/**
 * @brief 可变参数模板：打印所有参数
 *
 * 递归终止函数：处理最后一个参数
 */
void print_all() {
    std::cout << std::endl;
}

/**
 * @brief 可变参数模板：递归打印参数
 *
 * 每次递归处理一个参数，然后递归调用处理剩余参数。
 *
 * @tparam Head 第一个参数类型
 * @tparam Tail 剩余参数类型
 * @param head 第一个参数的值
 * @param tail 剩余参数的值
 */
template <typename Head, typename... Tail>
void print_all(Head head, Tail... tail) {
    std::cout << head << " ";
    print_all(tail...);
}

/**
 * @brief 可变参数模板：计算参数之和
 *
 * 递归累加所有参数值。
 *
 * @tparam T 参数类型（所有参数必须是同类型）
 * @param args 参数包
 * @return 所有参数的和
 */
template <typename T>
T sum_all(T value) {
    return value;
}

template <typename T, typename... Args>
T sum_all(T first, Args... args) {
    return first + sum_all(args...);
}

// ============================================================
// 第五部分：constexpr if（C++17）
// ============================================================

/**
 * @brief C++17 constexpr if 示例
 *
 * constexpr if 允许在编译时根据条件选择代码路径，
 * 避免了 SFINAE 的复杂性。
 *
 * @tparam T 类型参数
 * @param value 输入值
 * @return 处理后的字符串表示
 */
template <typename T>
std::string inspect_type(T /*value*/) {
    // 编译器在编译时选择分支，生成的代码不包含被排除的分支
    if constexpr (std::is_integral_v<T>) {
        return "整数类型 (integral)";
    } else if constexpr (std::is_floating_point_v<T>) {
        return "浮点类型 (floating point)";
    } else if constexpr (is_iterable_v<T>) {
        return "可迭代类型 (iterable)";
    } else {
        return "其他类型 (other)";
    }
}

// ============================================================
// 第六部分：模板与继承
// ============================================================

/**
 * @brief 模板基类
 *
 * 展示 CRTP（Curiously Recurring Template Pattern）模式。
 * 子类继承自模板化的基类，基类通过模板参数访问子类的成员。
 *
 * @tparam Derived 子类类型（递归继承）
 */
template <typename Derived>
class Base {
public:
    // 通用接口，调用子类的实现
    void execute() {
        // 静态多态：通过隐式转换访问子类成员
        static_cast<Derived*>(this)->do_execute();
    }

    // 子类需要实现的接口（可选强制）
    void do_execute() {
        std::cout << "Base::do_execute()" << std::endl;
    }
};

/**
 * @brief 具体子类
 */
class Derived : public Base<Derived> {
public:
    void do_execute() {
        std::cout << "Derived::do_execute() - 自定义实现" << std::endl;
    }
};

// ============================================================
// 主函数：演示所有模板特性
// ============================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "     C++ 模板元编程演示" << std::endl;
    std::cout << "========================================" << std::endl;

    // 1. 函数模板演示
    std::cout << "\n[1] 函数模板" << std::endl;
    std::cout << "max_value(3, 7) = " << max_value(3, 7) << std::endl;
    std::cout << "max_value(3.14, 2.71) = " << max_value(3.14, 2.71) << std::endl;

    // 2. 类模板演示
    std::cout << "\n[2] 类模板 (Stack)" << std::endl;
    Stack<int, 5> int_stack;
    int_stack.push(10);
    int_stack.push(20);
    int_stack.push(30);
    std::cout << "栈大小: " << int_stack.size() << std::endl;
    std::cout << "栈顶: " << int_stack.top() << std::endl;
    std::cout << "弹出: " << int_stack.pop() << std::endl;

    // 3. SFINAE 演示
    std::cout << "\n[3] SFINAE 与类型检测" << std::endl;
    std::cout << "is_iterable<vector>: " << is_iterable<std::vector<int>>::value << std::endl;
    std::cout << "is_iterable<int>: " << is_iterable<int>::value << std::endl;

    // 4. 可变参数模板演示
    std::cout << "\n[4] 可变参数模板" << std::endl;
    std::cout << "print_all: ";
    print_all(1, 2.5, "hello", 'A');
    std::cout << "sum_all(1, 2, 3, 4, 5) = " << sum_all(1, 2, 3, 4, 5) << std::endl;

    // 5. constexpr if 演示
    std::cout << "\n[5] constexpr if" << std::endl;
    std::cout << "int: " << inspect_type(42) << std::endl;
    std::cout << "double: " << inspect_type(3.14) << std::endl;
    std::cout << "string: " << inspect_type(std::string("test")) << std::endl;

    // 6. 模板与继承（CRTP）演示
    std::cout << "\n[6] CRTP 模板模式" << std::endl;
    Derived d;
    d.execute();  // 调用的是 Derived::do_execute()

    std::cout << "\n========================================" << std::endl;
    std::cout << "演示完成！" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
