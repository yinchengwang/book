/**
 * @file main.cpp
 * @brief 创建型设计模式演示
 *
 * 涵盖：单例、工厂、抽象工厂、建造者、原型模式
 * 编译：g++ -std=c++17 -o main main.cpp
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>

using std::cout;
using std::endl;
using std::make_unique;
using std::unique_ptr;
using std::vector;

// ============================================================================
// 1. 单例模式 (Singleton)
// ============================================================================
class Singleton {
public:
    static Singleton& getInstance() {
        static Singleton instance;  // 局部静态变量，线程安全 (C++11+)
        return instance;
    }

    void operation() const { cout << "Singleton::operation()" << endl; }

private:
    Singleton() = default;                    // 私有构造
    Singleton(const Singleton&) = delete;     // 禁用拷贝
    Singleton& operator=(const Singleton&) = delete;  // 禁用赋值
};

// ============================================================================
// 2. 工厂方法模式 (Factory Method)
// ============================================================================
class Product {
public:
    virtual ~Product() = default;
    virtual void use() const = 0;
};

class ConcreteProductA : public Product {
public:
    void use() const override { cout << "Using ConcreteProductA" << endl; }
};

class ConcreteProductB : public Product {
public:
    void use() const override { cout << "Using ConcreteProductB" << endl; }
};

// 工厂基类
class Factory {
public:
    virtual ~Factory() = default;
    virtual unique_ptr<Product> createProduct() const = 0;
};

// 具体工厂
class FactoryA : public Factory {
public:
    unique_ptr<Product> createProduct() const override {
        return make_unique<ConcreteProductA>();
    }
};

class FactoryB : public Factory {
public:
    unique_ptr<Product> createProduct() const override {
        return make_unique<ConcreteProductB>();
    }
};

// ============================================================================
// 3. 抽象工厂模式 (Abstract Factory)
// ============================================================================
class Button {
public:
    virtual ~Button() = default;
    virtual void render() const = 0;
};

class WinButton : public Button {
public:
    void render() const override { cout << "Rendering Windows Button" << endl; }
};

class MacButton : public Button {
public:
    void render() const override { cout << "Rendering Mac Button" << endl; }
};

class Checkbox {
public:
    virtual ~Checkbox() = default;
    virtual void render() const = 0;
};

class WinCheckbox : public Checkbox {
public:
    void render() const override { cout << "Rendering Windows Checkbox" << endl; }
};

class MacCheckbox : public Checkbox {
public:
    void render() const override { cout << "Rendering Mac Checkbox" << endl; }
};

// 抽象工厂接口
class GUIFactory {
public:
    virtual ~GUIFactory() = default;
    virtual unique_ptr<Button> createButton() const = 0;
    virtual unique_ptr<Checkbox> createCheckbox() const = 0;
};

// 具体工厂
class WinFactory : public GUIFactory {
public:
    unique_ptr<Button> createButton() const override { return make_unique<WinButton>(); }
    unique_ptr<Checkbox> createCheckbox() const override { return make_unique<WinCheckbox>(); }
};

class MacFactory : public GUIFactory {
public:
    unique_ptr<Button> createButton() const override { return make_unique<MacButton>(); }
    unique_ptr<Checkbox> createCheckbox() const override { return make_unique<MacCheckbox>(); }
};

// ============================================================================
// 4. 建造者模式 (Builder)
// ============================================================================
class Pizza {
public:
    void setDough(const std::string& d) { dough_ = d; }
    void setSauce(const std::string& s) { sauce_ = s; }
    void setTopping(const std::string& t) { topping_ = t; }

    void show() const {
        cout << "Pizza with " << dough_ << " dough, " << sauce_ << " sauce, " << topping_ << " topping" << endl;
    }

private:
    std::string dough_, sauce_, topping_;
};

class PizzaBuilder {
public:
    virtual ~PizzaBuilder() = default;

    PizzaBuilder& buildDough() {
        pizza_.setDough("thin");
        return *this;
    }

    PizzaBuilder& buildSauce() {
        pizza_.setSauce("tomato");
        return *this;
    }

    PizzaBuilder& buildTopping() {
        pizza_.setTopping("cheese");
        return *this;
    }

    Pizza build() { return std::move(pizza_); }

protected:
    Pizza pizza_;
};

// ============================================================================
// 5. 原型模式 (Prototype)
// ============================================================================
class Prototype {
public:
    virtual ~Prototype() = default;
    virtual unique_ptr<Prototype> clone() const = 0;
    virtual void show() const = 0;
};

class ConcretePrototype : public Prototype {
public:
    ConcretePrototype(int v) : value_(v) {}

    unique_ptr<Prototype> clone() const override {
        return make_unique<ConcretePrototype>(*this);  // 拷贝构造
    }

    void show() const override { cout << "ConcretePrototype value: " << value_ << endl; }

private:
    int value_;
};

// ============================================================================
// 客户端代码
// ============================================================================
int main() {
    cout << "=== C++ 创建型设计模式演示 ===" << endl << endl;

    // 1. 单例模式
    cout << "1. Singleton:" << endl;
    Singleton::getInstance().operation();

    // 2. 工厂方法
    cout << "\n2. Factory Method:" << endl;
    unique_ptr<Factory> factoryA = make_unique<FactoryA>();
    unique_ptr<Product> product = factoryA->createProduct();
    product->use();

    // 3. 抽象工厂
    cout << "\n3. Abstract Factory:" << endl;
    unique_ptr<GUIFactory> guiFactory = make_unique<WinFactory>();
    unique_ptr<Button> button = guiFactory->createButton();
    button->render();

    // 4. 建造者
    cout << "\n4. Builder:" << endl;
    PizzaBuilder builder;
    Pizza pizza = builder.buildDough().buildSauce().buildTopping().build();
    pizza.show();

    // 5. 原型
    cout << "\n5. Prototype:" << endl;
    ConcretePrototype original(42);
    auto cloned = original.clone();
    cloned->show();

    return 0;
}
