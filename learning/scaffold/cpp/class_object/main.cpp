// class_object scaffold — 类与对象
//
// 复现命令：
//   g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
//
// 演示 5 段：
//   [ctor]       — 构造函数与析构函数
//   [initlist]   — 初始化列表 vs 体内赋值
//   [const]      — const 成员函数
//   [static]     — static 成员（类级共享）
//   [this-friend] — this 指针与 friend 友元

#include <iostream>
#include <string>
#include <cstdio>

class Point {
public:
    // [ctor] 构造函数 + 静态计数
    Point() : x_(0), y_(0) {
        printf("  [ctor] Point() default at %p\n", this);
        instances_++;
    }

    Point(double x, double y) : x_(x), y_(y) {
        printf("  [ctor] Point(%.1f, %.1f)\n", x, y);
        instances_++;
    }

    // 拷贝构造函数
    Point(const Point& other) : x_(other.x_), y_(other.y_) {
        printf("  [ctor] Point copy at %p\n", this);
        instances_++;
    }

    // 析构函数
    ~Point() {
        printf("  [dtor] ~Point() at %p\n", this);
        instances_--;
    }

    // [const] const 成员函数（不修改状态）
    double x() const { return x_; }
    double y() const { return y_; }

    // [this] this 指针
    Point& shift(double dx, double dy) {
        this->x_ += dx;            // this 指向当前对象
        this->y_ += dy;
        return *this;              // 支持链式调用
    }

    void print(const char* tag) const {
        printf("  [%s] Point(%.2f, %.2f)\n", tag, x_, y_);
    }

    // [static] 静态成员函数（无 this）
    static int count() { return instances_; }

private:
    double x_;
    double y_;
    static int instances_;          // 类级共享

    friend class PointPair;         // 友元类声明
};

// 静态成员定义（类外）
int Point::instances_ = 0;

// [friend] 友元类/函数
class PointPair {
public:
    PointPair(const Point& a, const Point& b) : a_(a), b_(b) {}

    double manhattan() const {
        // 友元访问 Point 的 private 成员
        return std::abs(a_.x_ - b_.x_) + std::abs(a_.y_ - b_.y_);
    }

    void print(const char* tag) const {
        printf("  [%s] distance = %.2f\n", tag, manhattan());
    }

private:
    const Point& a_;
    const Point& b_;
};

int main() {
    // === [ctor] 构造函数与析构 ===
    printf("[ctor] === 构造函数与析构 ===\n");
    Point p1;                       // 默认构造
    p1.print("p1");
    Point p2(3.0, 4.0);            // 双参构造
    p2.print("p2");
    printf("  Point 实例数 = %d\n", Point::count());

    {
        Point p3(1.0, 1.0);        // 块作用域
        p3.print("p3");
        printf("  Point 实例数 (p3 创建后) = %d\n", Point::count());
    }                                // p3 析构
    printf("  Point 实例数 (p3 析构后) = %d\n", Point::count());

    // === [initlist] 初始化列表 vs 体内赋值 ===
    printf("\n[initlist] === 初始化列表 vs 体内赋值 ===\n");
    // 初始化列表：直接构造，对 const/引用成员必需
    // 体内赋值：先默认构造，再赋值（浪费一次构造）
    Point p4(5.0, 6.0);
    p4.print("p4 (initlist)");

    // === [const] const 成员函数 ===
    printf("\n[const] === const 成员函数 ===\n");
    const Point p_const(7.0, 8.0);
    printf("  const Point: x=%.2f, y=%.2f (通过 const 成员函数读)\n",
           p_const.x(), p_const.y());
    // p_const.shift(1, 1);  // 编译错误：不能在 const 对象上调非 const 函数

    // === [static] static 成员 ===
    printf("\n[static] === static 成员（类级共享）===\n");
    printf("  当前实例数 = %d\n", Point::count());

    // === [this] this 与链式调用 ===
    printf("\n[this] === this 指针与链式调用 ===\n");
    Point p5(0, 0);
    p5.shift(1, 1).shift(2, 2).shift(3, 3);
    p5.print("p5 after 3 shifts");   // (6, 6)

    // === [friend] 友元类 ===
    printf("\n[friend] === friend 友元类 ===\n");
    PointPair pair1(p1, p2);
    pair1.print("pair1 manhattan");
    PointPair pair2(Point(0, 0), Point(3, 4));
    pair2.print("pair2 manhattan");  // 7

    printf("\n=== PASS ===\n");
    return 0;
}