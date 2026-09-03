/**
 * @file main.cpp
 * @brief 代码质量工具 clang-tidy 演示
 *
 * 演示：modernize-use-* + performance-* 检查
 * 运行：clang-tidy main.cpp -- -std=c++17
 * 编译：g++ -std=c++17 -o main main.cpp
 */

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>

// ============================================================================
// 1. modernize-use-nullptr
// ============================================================================
void demoModernizeNullptr() {
    // 旧代码：使用 NULL 或 0
    int* ptr1 = NULL;
    int* ptr2 = 0;

    // 新代码：使用 nullptr
    int* ptr3 = nullptr;

    // clang-tidy 会建议：modernize-use-nullptr
    (void)ptr1;
    (void)ptr2;
    (void)ptr3;
}

// ============================================================================
// 2. modernize-use-emplace
// ============================================================================
class HeavyObject {
public:
    HeavyObject(int x) : x_(x) { std::cout << "Construct: " << x_ << std::endl; }
    ~HeavyObject() { std::cout << "Destruct: " << x_ << std::endl; }
    int getX() const { return x_; }

private:
    int x_;
};

void demoModernizeEmplace() {
    std::vector<std::string> v;

    // 旧代码：先构造再拷贝
    v.push_back(std::string("hello"));
    v.push_back("world");

    // 新代码：直接构造
    v.emplace_back("emplace");
    v.emplace_back(5, 'x');  // 构造 "xxxxx"

    // clang-tidy 会建议：modernize-use-emplace
}

// ============================================================================
// 3. modernize-use-auto
// ============================================================================
void demoModernizeAuto() {
    std::vector<int> demoVec = {1, 2, 3};

    // 旧代码：显式类型
    std::vector<int>::iterator it1 = demoVec.begin();
    std::vector<int>::const_iterator cit1 = demoVec.cbegin();

    // 新代码：使用 auto
    auto it2 = demoVec.begin();
    auto cit2 = demoVec.cend();

    // clang-tidy 会建议：modernize-use-auto
    (void)it1; (void)cit1; (void)it2; (void)cit2;
}

// ============================================================================
// 4. performance-* 检查
// ============================================================================
std::vector<int> globalVec = {1, 2, 3, 4, 5};

void demoPerformanceChecks() {
    std::vector<int> localVec = {1, 2, 3, 4, 5};

    // performance-unnecessary-copy-initialization
    // 旧代码：拷贝整个 vector
    // auto copy = localVec;  // 不必要的拷贝

    // performance-move-const-arg
    // 如果函数接受右值引用，传递临时对象

    // performance-for-range-copy
    // 旧代码：拷贝每个元素
    // for (int x : localVec) { }

    // 新代码：使用引用
    for (const auto& x : localVec) {
        (void)x;
    }
}

// ============================================================================
// 5. readability-* 检查
// ============================================================================
class ReadabilityDemo {
public:
    // readability-conversion-numeric
    // 避免隐式数值转换
    void convert(double d) {
        int i = static_cast<int>(d);  // 显式转换
        (void)i;
    }

    // readability-redundant-string-init
    std::string redundantInit() {
        std::string s("literal");  // 可以直接用 = 初始化
        return s;
    }

    // readability-isolate-declaration
    void multipleDecl() {
        int a = 1, b = 2, c = 3;  // readability-isolate-declaration
        (void)a; (void)b; (void)c;
    }
};

// ============================================================================
// 6. modernize-use-override
// ============================================================================
class Base {
public:
    virtual ~Base() = default;
    virtual void virtualMethod() {}
    virtual int anotherMethod(int x) { return x; }
};

class Derived : public Base {
public:
    // 应该加 override
    void virtualMethod() override {}
    int anotherMethod(int x) override { return x * 2; }
};

// ============================================================================
// 7. bugprone-* 检查
// ============================================================================
class BugproneDemo {
public:
    // bugprone-unused-local-non-trivial-variable
    void unusedVar() {
        auto result = std::make_unique<int>(42);
        // 如果 result 没有被使用，clang-tidy 会警告
    }

    // bugprone-forward-declaration-namespace
    // 避免前向声明在错误命名空间

    // bugprone-suspicious-semicolon
    void suspiciousSemicolon() {
        if (false);  // 空语句，clang-tidy 会警告
            std::cout << "Never printed" << std::endl;
    }
};

// ============================================================================
// 客户端代码
// ============================================================================
using std::vector;

int main() {
    std::cout << "=== clang-tidy 演示 ===" << std::endl << std::endl;

    std::cout << "1. modernize-use-nullptr:" << std::endl;
    demoModernizeNullptr();
    std::cout << "   建议：使用 nullptr 替代 NULL 或 0" << std::endl;

    std::cout << "\n2. modernize-use-emplace:" << std::endl;
    demoModernizeEmplace();
    std::cout << "   建议：使用 emplace_back 替代 push_back" << std::endl;

    std::cout << "\n3. modernize-use-auto:" << std::endl;
    std::vector<int> v = {1, 2, 3};
    demoModernizeAuto();
    std::cout << "   建议：使用 auto 简化类型声明" << std::endl;

    std::cout << "\n4. performance-*:" << std::endl;
    demoPerformanceChecks();
    std::cout << "   建议：检查性能相关问题" << std::endl;

    std::cout << "\n5. readability-*:" << std::endl;
    ReadabilityDemo rd;
    rd.convert(3.14);
    std::cout << "   建议：提高代码可读性" << std::endl;

    std::cout << "\n6. modernize-use-override:" << std::endl;
    Derived d;
    std::cout << "   建议：派生类重写方法加 override" << std::endl;

    std::cout << "\n7. bugprone-*:" << std::endl;
    BugproneDemo bd;
    bd.suspiciousSemicolon();
    std::cout << "   建议：修复潜在的 bug" << std::endl;

    std::cout << "\n=== 运行 clang-tidy ===" << std::endl;
    std::cout << "clang-tidy main.cpp -- -std=c++17" << std::endl;
    std::cout << "\n常用检查项:" << std::endl;
    std::cout << "  modernize-*" << std::endl;
    std::cout << "  performance-*" << std::endl;
    std::cout << "  readability-*" << std::endl;
    std::cout << "  bugprone-*" << std::endl;

    return 0;
}
