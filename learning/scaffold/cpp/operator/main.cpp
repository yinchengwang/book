// operator scaffold — 运算符重载
//
// 复现命令：
//   g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
//
// 演示 5 段：
//   [member]   — 成员运算符重载（+=, <, []）
//   [nonmem]   — 非成员运算符重载（+, <<, ==）
//   [stream]   — 流运算符 << >>
//   [cast]     — 类型转换运算符
//   [explicit] — explicit 阻止隐式转换

#include <iostream>
#include <string>
#include <cstdio>
#include <cmath>

class Vec2 {
public:
    double x, y;

    Vec2() : x(0), y(0) {}
    Vec2(double x_, double y_) : x(x_), y(y_) {}

    // [member] += 成员运算符
    Vec2& operator+=(const Vec2& rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    // [member] < 字典序比较
    bool operator<(const Vec2& rhs) const {
        return (x < rhs.x) || (x == rhs.x && y < rhs.y);
    }

    // [member] [] 下标访问
    double& operator[](size_t i) {
        return i == 0 ? x : y;
    }
    const double& operator[](size_t i) const {
        return i == 0 ? x : y;
    }

    // [cast] explicit 类型转换运算符（C++11）
    explicit operator double() const {
        return std::sqrt(x * x + y * y);     // 模长
    }

    void print(const char* tag) const {
        printf("  [%s] Vec2(%.2f, %.2f)\n", tag, x, y);
    }
};

// [nonmem] + 二元运算符（非成员，因为左操作数可能是 rvalue）
Vec2 operator+(const Vec2& a, const Vec2& b) {
    Vec2 r = a;
    r += b;
    return r;
}

// [nonmem] == 相等
bool operator==(const Vec2& a, const Vec2& b) {
    return a.x == b.x && a.y == b.y;
}

// [stream] << 流输出
std::ostream& operator<<(std::ostream& os, const Vec2& v) {
    os << "Vec2(" << v.x << ", " << v.y << ")";
    return os;
}

// [stream] >> 流输入
std::istream& operator>>(std::istream& is, Vec2& v) {
    is >> v.x >> v.y;
    return is;
}

// [nonmem] 友元 << 演示（用 printf 风格）
void operator<<(Vec2& v, const char* tag) {
    printf("  [%s] Vec2(%.2f, %.2f)\n", tag, v.x, v.y);
}

int main() {
    // === [member] 成员运算符重载 ===
    printf("[member] === 成员运算符重载 ===\n");
    Vec2 a(3.0, 4.0);
    Vec2 b(1.0, 2.0);
    a.print("a");
    b.print("b");

    a += b;                       // 调用 operator+=
    a.print("a += b");

    if (b < a) printf("  b < a 成立（字典序）\n");

    // [] 访问
    printf("  a[0] = %.2f, a[1] = %.2f\n", a[0], a[1]);
    a[0] = 100.0;
    a.print("a[0]=100");

    // === [nonmem] 非成员运算符 ===
    printf("\n[nonmem] === 非成员运算符重载 ===\n");
    Vec2 c(5.0, 6.0);
    Vec2 d = c + Vec2(1.0, 1.0);  // operator+
    d.print("d = c + (1,1)");

    if (c == Vec2(5.0, 6.0)) {
        printf("  c == (5, 6) 成立\n");
    }

    // === [stream] 流运算符 ===
    printf("\n[stream] === 流运算符 << >> ===\n");
    std::cout << "  cout: " << c << std::endl;
    Vec2 e;
    std::cout << "  输入 Vec2 (x y): ";
    std::cin >> e;
    std::cout << "  你输入了: " << e << std::endl;

    // === [cast] 类型转换 ===
    printf("\n[cast] === 类型转换运算符 ===\n");
    Vec2 v3(3.0, 4.0);
    // double len = v3;  // 编译错误：explicit 阻止隐式转换
    double len = static_cast<double>(v3);   // 显式转换 OK
    printf("  static_cast<double>(Vec2(3,4)) = %.2f (3-4-5 直角三角)\n", len);

    // === [explicit] explicit 防隐式转换 ===
    printf("\n[explicit] === explicit 阻止隐式转换 ===\n");
    // void foo(double d);  foo(v3);  // 编译错误：Vec2 → double 不允许隐式
    // 必须 static_cast<double>(v3) 或显式构造

    printf("\n=== PASS ===\n");
    return 0;
}