/**
 * @file main.cpp
 * @brief Lambda 表达式完整演示
 *
 * 本文件演示 C++ Lambda 表达式的核心特性：
 * 1. 捕获列表（capture list）
 * 2. 参数列表与返回值类型
 * 3. 泛型 Lambda（C++14+）
 * 4. Lambda 与 STL 算法的配合使用
 *
 * 编译：g++ -std=c++17 -Wall -Wextra -g -o test main.cpp
 */

#include <algorithm>  // std::for_each, std::transform, std::find_if
#include <functional> // std::function, std::bind
#include <iostream>   // std::cout, std::endl
#include <memory>     // std::unique_ptr
#include <numeric>    // std::accumulate
#include <string>     // std::string
#include <vector>     // std::vector

// =============================================================================
// 辅助函数：打印分隔线
// =============================================================================
void print_separator(const std::string& title) {
    std::cout << "\n========== " << title << " ==========\n";
}

// =============================================================================
// 演示 1：Lambda 基础语法与捕获列表
// =============================================================================
void demo_capture_list() {
    print_separator("Lambda 捕获列表演示");

    int x = 10;
    int y = 20;

    // 1. 不捕获（无依赖外部变量）
    auto add = [](int a, int b) { return a + b; };
    std::cout << "无捕获: add(3, 5) = " << add(3, 5) << std::endl;

    // 2. 按值捕获 [=]（捕获所有外部变量的副本）
    // 捕获后，lambda 内部使用的是捕获时的快照
    auto by_value = [=]() { return x + y; };
    std::cout << "按值捕获 [=]: x + y = " << by_value() << std::endl;

    // 3. 按引用捕获 [&]（捕获所有外部变量的引用）
    // 注意：lambda 捕获的引用在执行时必须有效
    auto by_reference = [&]() {
        // 此处故意注释掉修改，以避免警告
        // x += 10;  // 如果取消注释，外部 x 会被修改
        return x + y;
    };
    std::cout << "按引用捕获 [&]: x + y = " << by_reference() << std::endl;

    // 4. 显式指定捕获特定变量
    auto mix_capture = [x, &y]() {
        // x 按值，y 按引用
        // x = 100;  // 错误：按值捕获的变量是 const
        y = 100;    // OK：按引用捕获可以直接修改
        return x + y;
    };
    std::cout << "混合捕获 [x, &y]: 修改变量 y 后 = " << mix_capture() << std::endl;
    std::cout << "外部 y 已被修改为: " << y << std::endl;

    // 5. mutable Lambda：允许在按值捕获的 lambda 内部修改捕获的副本
    int counter = 0;
    auto mutable_counter = [counter]() mutable {
        counter = 100;  // 修改的是副本，不影响外部 counter
        return counter;
    };
    std::cout << "mutable Lambda: 返回 " << mutable_counter()
              << "，外部 counter 仍为 " << counter << std::endl;

    // 6. 初始化捕获（C++14+）：捕获表达式的计算结果
    auto init_capture = [z = x * 2]() { return z; };
    std::cout << "初始化捕获: z = x * 2 = " << init_capture() << std::endl;
}

// =============================================================================
// 演示 2：Lambda 返回值类型
// =============================================================================
void demo_return_type() {
    print_separator("Lambda 返回值类型演示");

    // 1. 自动推导返回值类型（常用）
    auto square = [](int n) { return n * n; };
    std::cout << "自动推导: square(5) = " << square(5) << std::endl;

    // 2. 显式指定返回值类型（后置返回类型语法）
    auto safe_divide = [](double a, double b) -> double {
        if (b == 0.0) return 0.0;  // 避免除零
        return a / b;
    };
    std::cout << "显式返回: safe_divide(10, 3) = " << safe_divide(10, 3) << std::endl;

    // 3. 条件表达式需要显式声明返回类型（因为条件运算符会产生类型不匹配）
    auto classify = [](int n) -> std::string {
        if (n > 0) return "positive";
        if (n < 0) return "negative";
        return "zero";
    };
    std::cout << "条件返回: classify(-5) = " << classify(-5) << std::endl;

    // 4. 返回 void 的 lambda（用于副作用）
    int count = 0;
    auto increment = [&count](int n) {
        count += n;
    };
    increment(42);
    std::cout << "void 返回: count += 42 后，count = " << count << std::endl;
}

// =============================================================================
// 演示 3：泛型 Lambda（C++14+）
// =============================================================================
void demo_generic_lambda() {
    print_separator("泛型 Lambda 演示（C++14）");

    // 1. 参数类型自动推导（不需要写模板）
    auto print = [](auto value) {
        std::cout << "值: " << value << std::endl;
    };
    print(42);           // int
    print(3.14);          // double
    print("hello");       // const char*
    print(std::string("world"));  // std::string

    // 2. 泛型 lambda 用于容器操作
    std::vector<int> nums = {1, 2, 3, 4, 5};
    std::vector<double> doubles = {1.1, 2.2, 3.3};

    // 同一个 lambda 可以处理不同类型
    auto twice = [](auto n) { return n * 2; };

    std::vector<int> doubled_nums;
    std::transform(nums.begin(), nums.end(), std::back_inserter(doubled_nums), twice);
    std::cout << "整数翻倍: ";
    for (int n : doubled_nums) std::cout << n << " ";
    std::cout << std::endl;

    std::vector<double> doubled_doubles;
    std::transform(doubles.begin(), doubles.end(), std::back_inserter(doubled_doubles), twice);
    std::cout << "浮点翻倍: ";
    for (double d : doubled_doubles) std::cout << d << " ";
    std::cout << std::endl;

    // 3. 泛型 lambda 的参数包展开（C++17 constexpr if 配合）
    // 注意：这里演示参数推导，实际使用中可用 std::initializer_list
    auto sum = [](auto... args) {
        // C++17 特性，这里仅作概念演示
        // return (... + args);  // C++17 折叠表达式
        return (0 + ... + args);  // C++17 右折叠
    };
    std::cout << "可变参数求和: sum(1, 2, 3, 4, 5) = " << sum(1, 2, 3, 4, 5) << std::endl;
}

// =============================================================================
// 演示 4：Lambda 与 STL 算法配合
// =============================================================================
void demo_stl_algorithms() {
    print_separator("Lambda 与 STL 算法配合");

    std::vector<int> data = {5, 2, 8, 1, 9, 3, 7, 4, 6};

    // 1. std::for_each：遍历并执行操作
    std::cout << "for_each 遍历: ";
    std::for_each(data.begin(), data.end(), [](int n) {
        std::cout << n << " ";
    });
    std::cout << std::endl;

    // 2. std::transform：转换元素
    std::vector<int> squared;
    squared.reserve(data.size());
    std::transform(data.begin(), data.end(), std::back_inserter(squared),
                   [](int n) { return n * n; });
    std::cout << "transform 平方: ";
    for (int n : squared) std::cout << n << " ";
    std::cout << std::endl;

    // 3. std::find_if：查找第一个满足条件的元素
    auto it = std::find_if(data.begin(), data.end(), [](int n) {
        return n > 5;
    });
    if (it != data.end()) {
        std::cout << "find_if 首个 > 5: " << *it << std::endl;
    }

    // 4. std::count_if：计数满足条件的元素
    int evens = std::count_if(data.begin(), data.end(), [](int n) {
        return n % 2 == 0;
    });
    std::cout << "count_if 偶数个数: " << evens << std::endl;

    // 5. std::sort：自定义排序规则
    std::vector<int> sorted = data;
    std::sort(sorted.begin(), sorted.end(), [](int a, int b) {
        return a > b;  // 降序排列
    });
    std::cout << "sort 降序排列: ";
    for (int n : sorted) std::cout << n << " ";
    std::cout << std::endl;

    // 6. std::remove_if + erase 惯用法：删除满足条件的元素
    std::vector<int> filtered = data;
    filtered.erase(std::remove_if(filtered.begin(), filtered.end(),
                                   [](int n) { return n < 4; }),
                   filtered.end());
    std::cout << "remove_if 删除 < 4: ";
    for (int n : filtered) std::cout << n << " ";
    std::cout << std::endl;

    // 7. std::accumulate：自定义聚合操作
    int sum = std::accumulate(data.begin(), data.end(), 0, [](int acc, int n) {
        return acc + n;
    });
    std::cout << "accumulate 求和: " << sum << std::endl;

    // 8. std::any_of / all_of / none_of：范围断言
    bool any_positive = std::any_of(data.begin(), data.end(), [](int n) { return n > 0; });
    bool all_positive = std::all_of(data.begin(), data.end(), [](int n) { return n > 0; });
    std::cout << "any_of 正数: " << (any_positive ? "是" : "否") << std::endl;
    std::cout << "all_of 正数: " << (all_positive ? "是" : "否") << std::endl;
}

// =============================================================================
// 演示 5：Lambda 作为回调函数
// =============================================================================
void demo_callback() {
    print_separator("Lambda 作为回调函数");

    // 模拟一个接受回调的函数（使用 std::function 支持空检查）
    auto process_with_callback = [](int n, std::function<int(int)> callback) {
        std::cout << "处理: " << n;
        if (callback) {
            int result = callback(n);
            std::cout << " -> 回调结果: " << result;
        }
        std::cout << std::endl;
    };

    // 使用 lambda 作为回调
    process_with_callback(5, [](int x) { return x * x; });
    process_with_callback(10, [](int x) { return x + 100; });

    // 捕获外部状态的回调
    int multiplier = 3;
    process_with_callback(7, [&multiplier](int x) {
        return x * multiplier;
    });

    // std::function 存储带状态的 lambda
    std::function<int(int)> make_multiplier = [&multiplier](int x) -> int {
        return x * multiplier;
    };
    std::cout << "std::function 回调: " << make_multiplier(6) << std::endl;
}

// =============================================================================
// 主函数
// =============================================================================
int main() {
    std::cout << "========================================\n";
    std::cout << "  C++ Lambda 表达式完整演示\n";
    std::cout << "  编译器标准: C++17\n";
    std::cout << "========================================\n";

    // 演示各部分功能
    demo_capture_list();       // 捕获列表
    demo_return_type();       // 返回值类型
    demo_generic_lambda();    // 泛型 Lambda
    demo_stl_algorithms();     // STL 算法配合
    demo_callback();          // 回调函数

    std::cout << "\n========== 演示结束 ==========\n";
    return 0;
}
