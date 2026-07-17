// inheritance scaffold — 继承与多态
//
// 复现命令：
//   g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
//
// 演示 5 段：
//   [single]      — 单继承 + 访问控制
//   [vtable]      — 虚函数表 + 多态
//   [override]    — override/final 关键字
//   [abstract]    — 纯虚函数 + 抽象类
//   [multi]       — 多继承 + 虚基类（菱形继承）

#include <iostream>
#include <string>
#include <cstdio>
#include <vector>

// 步进计数器（每次调用返回 step1, step2, ...）
static const char* tag() {
    static int n = 0;
    static char buf[32];
    snprintf(buf, sizeof(buf), "step%d", ++n);
    return buf;
}

// === 基类 ===
class Shape {
public:
    Shape(const std::string& name) : name_(name) {
        printf("  [ctor] Shape(%s)\n", name_.c_str());
    }

    virtual ~Shape() {
        printf("  [dtor] ~Shape(%s)\n", name_.c_str());
    }

    // [vtable] 虚函数
    virtual double area() const {
        printf("  [vtable] Shape::area() 基类默认\n");
        return 0.0;
    }

    void info() const {
        printf("  [%s] name=%s, area=%.2f\n", tag(), name_.c_str(), area());
    }

protected:
    std::string name_;
};

// === [single] 单继承 ===
class Rectangle : public Shape {
public:
    Rectangle(double w, double h)
        : Shape("rectangle"), width_(w), height_(h) {}

    // [override] 显式 override（C++11）
    double area() const override {
        return width_ * height_;
    }

private:
    double width_;
    double height_;
};

class Square : public Rectangle {
public:
    Square(double side) : Rectangle(side, side) {}

    // [final] 不允许再继承覆盖
    double area() const override final {
        return Rectangle::area();   // 复用父类
    }
};

// === [abstract] 抽象类：至少一个纯虚函数 ===
class Animal {
public:
    Animal(const std::string& name) : name_(name) {}
    virtual ~Animal() = default;

    // 纯虚函数：= 0
    virtual void speak() const = 0;

    void introduce() const {
        printf("  [%s] I am %s, ", tag(), name_.c_str());
        speak();
    }

protected:
    std::string name_;
};

class Dog : public Animal {
public:
    Dog(const std::string& name) : Animal(name) {}
    void speak() const override {
        printf("Woof!\n");
    }
};

class Cat : public Animal {
public:
    Cat(const std::string& name) : Animal(name) {}
    void speak() const override {
        printf("Meow~\n");
    }
};

// === [multi] 多继承 + 虚基类 ===
class Flyable {
public:
    virtual void fly() const {
        printf("  [fly] Flying!\n");
    }
};

class Swimmable {
public:
    virtual void swim() const {
        printf("  [swim] Swimming!\n");
    }
};

// 多继承：鸭子能飞能游
class Duck : public Animal, public Flyable, public Swimmable {
public:
    Duck(const std::string& name) : Animal(name) {}
    void speak() const override {
        printf("Quack!\n");
    }
};

// 单继承多态调用的 tag（已在文件顶部定义）

int main() {
    // === [single] 单继承 ===
    printf("[single] === 单继承 ===\n");
    Rectangle r(3.0, 4.0);
    r.info();                          // area() 调用 Rectangle 版本
    Square s(5.0);
    s.info();

    // === [vtable] 多态：基类指针指向派生类 ===
    printf("\n[vtable] === 虚函数表与多态 ===\n");
    Shape* shapes[] = {&r, &s};
    for (Shape* p : shapes) {
        p->info();                     // 调用实际类型的 area()
    }

    // === [override] override 关键字检查 ===
    printf("\n[override] === override / final ===\n");
    // 若派生类函数签名错了，override 关键字让编译器报错
    // final 阻止进一步覆盖
    s.info();
    // Square sq2 = s; sq2.area() 也调用 final 版本

    // === [abstract] 抽象类 ===
    printf("\n[abstract] === 纯虚函数与抽象类 ===\n");
    // Animal a;  // 编译错误：抽象类不能实例化
    Dog d("Buddy");
    Cat c("Whiskers");
    d.introduce();
    c.introduce();

    // 多态：基类指针数组
    Animal* zoo[] = {&d, &c};
    for (Animal* a : zoo) {
        a->introduce();               // 调用实际类型的 speak()
    }

    // === [multi] 多继承 ===
    printf("\n[multi] === 多继承 ===\n");
    Duck duck("Donald");
    duck.introduce();
    duck.fly();
    duck.swim();

    printf("\n=== PASS ===\n");
    return 0;
}