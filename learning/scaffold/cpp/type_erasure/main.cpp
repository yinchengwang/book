/**
 * @file main.cpp
 * @brief 类型擦除演示
 *
 * 演示：std::function + std::any + 类型擦除原理
 * 编译：g++ -std=c++17 -o main main.cpp
 */

#include <iostream>
#include <functional>
#include <any>
#include <vector>
#include <string>
#include <memory>

using std::cout;
using std::endl;

// ============================================================================
// 1. std::function：函数对象的类型擦除
// ============================================================================
void callFunction(const std::function<void()>& f) {
    f();
}

auto lambda = []() { cout << "Lambda called" << endl; };

void regularFunction() { cout << "Function called" << endl; }

struct Functor {
    void operator()() const { cout << "Functor called" << endl; }
};

// ============================================================================
// 2. 类型擦除的简单实现
// ============================================================================
class AnyFunction {
public:
    // 接受任何可调用对象
    template<typename F>
    AnyFunction(F f) : impl_(std::make_unique<FunctionImpl<F>>(std::move(f))) {}

    void operator()() const { impl_->call(); }

private:
    struct Concept {
        virtual ~Concept() = default;
        virtual void call() const = 0;
    };

    template<typename F>
    struct FunctionImpl : Concept {
        explicit FunctionImpl(F f) : func(std::move(f)) {}
        void call() const override { func(); }
        F func;
    };

    std::unique_ptr<Concept> impl_;
};

// ============================================================================
// 3. std::any：任意类型的容器
// ============================================================================
void printAny(const std::any& a) {
    if (a.type() == typeid(int)) {
        cout << "int: " << std::any_cast<int>(a) << endl;
    } else if (a.type() == typeid(std::string)) {
        cout << "string: " << std::any_cast<std::string>(a) << endl;
    } else if (a.type() == typeid(double)) {
        cout << "double: " << std::any_cast<double>(a) << endl;
    } else {
        cout << "unknown type" << endl;
    }
}

// ============================================================================
// 4. 类型擦除的异构容器
// ============================================================================
class HeterogeneousVector {
public:
    template<typename T>
    void push(T value) {
        data_.push_back(std::make_any<T>(std::move(value)));
    }

    template<typename T>
    T get(size_t index) const {
        return std::any_cast<T>(data_[index]);
    }

    size_t size() const { return data_.size(); }

private:
    std::vector<std::any> data_;
};

// ============================================================================
// 5. 事件系统：类型擦除的应用
// ============================================================================
class EventBus {
public:
    // 订阅回调（简化版：只支持 void() 回调）
    void subscribe(std::function<void()> handler) {
        handlers_.push_back(std::move(handler));
    }

    void publish() {
        for (auto& h : handlers_) h();
    }

private:
    std::vector<std::function<void()>> handlers_;
};

// ============================================================================
// 6. 类型擦除的权衡
// ============================================================================
void demonstrateTradeoffs() {
    cout << "=== 类型擦除的权衡 ===" << endl;
    cout << "优点:" << endl;
    cout << "  - 异构容器：存储不同类型的对象" << endl;
    cout << "  - 统一接口：不同类型使用相同 API" << endl;
    cout << "  - 灵活性：运行时决定具体类型" << endl;
    cout << endl;
    cout << "缺点:" << endl;
    cout << "  - 性能开销：虚函数调用、类型转换" << endl;
    cout << "  - 类型安全：失去编译时类型检查" << endl;
    cout << "  - 调试困难：运行时错误更难定位" << endl;
}

// ============================================================================
// 客户端代码
// ============================================================================
int main() {
    cout << "=== 类型擦除演示 ===" << endl << endl;

    // 1. std::function
    cout << "1. std::function:" << endl;
    callFunction(lambda);
    callFunction(regularFunction);
    callFunction(Functor{});
    callFunction([]() { cout << "Inline lambda" << endl; });

    // 2. 自定义类型擦除
    cout << "\n2. Custom Type Erasure:" << endl;
    AnyFunction af1([]() { cout << "Af1 called" << endl; });
    AnyFunction af2(Functor{});
    af1();
    af2();

    // 3. std::any
    cout << "\n3. std::any:" << endl;
    std::any a1 = 42;
    std::any a2 = std::string("hello");
    std::any a3 = 3.14;
    printAny(a1);
    printAny(a2);
    printAny(a3);

    // 4. 异构容器
    cout << "\n4. Heterogeneous Container:" << endl;
    HeterogeneousVector hv;
    hv.push(10);
    hv.push(std::string("world"));
    hv.push(2.5f);
    cout << "int: " << hv.get<int>(0) << endl;
    cout << "string: " << hv.get<std::string>(1) << endl;
    cout << "float: " << hv.get<float>(2) << endl;

    // 5. 事件系统
    cout << "\n5. Event System:" << endl;
    EventBus bus;
    bus.subscribe([]() { cout << "Event 1" << endl; });
    bus.subscribe([]() { cout << "Event 2" << endl; });
    bus.publish();

    // 6. 权衡分析
    cout << "\n6. Tradeoffs:" << endl;
    demonstrateTradeoffs();

    return 0;
}
