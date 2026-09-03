/**
 * @file main.cpp
 * @brief CRTP（奇特的递归模板模式）演示
 *
 * 演示：静态多态、取消虚函数开销、编译期分发
 * 编译：g++ -std=c++17 -o main main.cpp
 */

#include <iostream>

using std::cout;
using std::endl;

// ============================================================================
// 1. 基本 CRTP：静态多态
// ============================================================================
template<typename Derived>
class Base {
public:
    void interface() {
        // 转换为派生类类型
        static_cast<Derived*>(this)->implementation();
    }

    // 通用方法，调用派生类的接口
    void commonOperation() {
        cout << "Base::commonOperation called" << endl;
        interface();
    }
};

class Derived1 : public Base<Derived1> {
public:
    void implementation() { cout << "Derived1::implementation" << endl; }
};

class Derived2 : public Base<Derived2> {
public:
    void implementation() { cout << "Derived2::implementation" << endl; }
};

// ============================================================================
// 2. CRTP 实现对象计数
// ============================================================================
template<typename T>
class Counter {
protected:
    static int objectCount;

public:
    Counter() { ++objectCount; }
    Counter(const Counter&) { ++objectCount; }

    static int getCount() { return objectCount; }
};

template<typename T>
int Counter<T>::objectCount = 0;

class CountedObject : public Counter<CountedObject> {
public:
    explicit CountedObject(int v) : value(v) {}
    int value;
};

// ============================================================================
// 3. CRTP 实现静态多态的克隆
// ============================================================================
template<typename Derived>
class Cloneable {
public:
    // 返回类型安全的克隆
    Derived* clone() const {
        return static_cast<const Derived*>(this)->cloneImpl();
    }
};

class Document : public Cloneable<Document> {
public:
    explicit Document(const std::string& content) : content_(content) {}
    Document* cloneImpl() const { return new Document(*this); }

    void print() const { cout << "Document: " << content_ << endl; }

private:
    std::string content_;
};

class Spreadsheet : public Cloneable<Spreadsheet> {
public:
    explicit Spreadsheet(int rows, int cols) : rows_(rows), cols_(cols) {}
    Spreadsheet* cloneImpl() const { return new Spreadsheet(*this); }

    void print() const { cout << "Spreadsheet: " << rows_ << "x" << cols_ << endl; }

private:
    int rows_, cols_;
};

// ============================================================================
// 4. CRTP 实现方法链式调用
// ============================================================================
template<typename Derived>
class Printable {
public:
    Derived& print(const std::string& msg) {
        cout << msg;
        return static_cast<Derived&>(*this);
    }

    Derived& println(const std::string& msg) {
        cout << msg << endl;
        return static_cast<Derived&>(*this);
    }
};

class Logger : public Printable<Logger> {
    // 自动获得链式调用能力
};

// ============================================================================
// 5. CRTP vs 虚函数性能对比
// ============================================================================
class VirtualBase {
public:
    virtual ~VirtualBase() = default;
    virtual void doSomething() = 0;
};

class VirtualDerived : public VirtualBase {
public:
    void doSomething() override { /* 空实现 */ }
};

template<typename Derived>
class StaticBase {
public:
    void doSomething() { static_cast<Derived*>(this)->doSomethingImpl(); }

protected:
    StaticBase() = default;
    StaticBase(const StaticBase&) {}
};

class StaticDerived : public StaticBase<StaticDerived> {
public:
    void doSomethingImpl() { /* 空实现 */ }
};

// ============================================================================
// 客户端代码
// ============================================================================
#include <string>

int main() {
    cout << "=== CRTP 演示 ===" << endl << endl;

    // 1. 静态多态
    cout << "1. Static Polymorphism:" << endl;
    Derived1 d1;
    Derived2 d2;
    d1.commonOperation();  // 调用 Derived1 的实现
    d2.commonOperation();  // 调用 Derived2 的实现

    // 2. 对象计数
    cout << "\n2. Object Counting:" << endl;
    CountedObject a(1), b(2), c(3);
    cout << "Object count: " << CountedObject::getCount() << endl;

    // 3. 类型安全的克隆
    cout << "\n3. Type-Safe Cloning:" << endl;
    Document doc("Hello");
    Spreadsheet ss(10, 5);
    doc.print();
    ss.print();
    auto clonedDoc = doc.clone();
    clonedDoc->print();
    delete clonedDoc;

    // 4. 链式调用
    cout << "\n4. Method Chaining:" << endl;
    Logger logger;
    logger.println("Line 1").print("No newline ").println("continued");

    // 5. CRTP vs 虚函数说明
    cout << "\n5. CRTP vs Virtual:" << endl;
    cout << "- CRTP: 编译时绑定，无虚表调用开销" << endl;
    cout << "- Virtual: 运行时绑定，有虚表查找开销" << endl;
    cout << "- CRTP 适用于：性能关键、内联优化、固定类型集合" << endl;
    cout << "- Virtual 适用于：运行时多态、插件架构、接口抽象" << endl;

    return 0;
}
