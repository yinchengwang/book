/**
 * @file main.cpp
 * @brief Pimpl（Pointer to Implementation）惯用法演示
 *
 * 演示：编译防火墙、ABI 稳定、头文件保护
 * 编译：g++ -std=c++17 -o main main.cpp
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

using std::cout;
using std::endl;

// ============================================================================
// 1. Pimpl 模式基础实现
// ============================================================================
class Widget {
public:
    Widget();                           // 构造
    ~Widget();                          // 析构（必须定义，因为 unique_ptr 需要完整类型）
    Widget(Widget&&);                   // 移动构造
    Widget& operator=(Widget&&);        // 移动赋值
    Widget(const Widget&) = delete;     // 禁用拷贝（可选）
    Widget& operator=(const Widget&) = delete;

    void setData(int val);
    int getData() const;

private:
    // Pimpl：指向实现类的指针
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

// ============================================================================
// 2. Pimpl 实现（隐藏细节）
// ============================================================================
struct Widget::Impl {
    // 包含所有私有成员
    int data_;
    std::string name_;
    std::vector<int> cache_;
    std::unordered_map<int, std::string> lookup_;

    Impl() : data_(0) {}
    Impl(int d) : data_(d) {}
};

Widget::Widget() : pImpl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default;
Widget::Widget(Widget&&) = default;
Widget& Widget::operator=(Widget&&) = default;

void Widget::setData(int val) { pImpl_->data_ = val; }
int Widget::getData() const { return pImpl_->data_; }

// ============================================================================
// 3. 不使用 Pimpl 的代价
// ============================================================================
class WidgetWithoutPimpl {
public:
    // 直接暴露所有依赖
    void setData(int val);

private:
    int data_;
    std::string name_;
    std::vector<int> cache_;
    std::unordered_map<int, std::string> lookup_;
    // 如果改变任何成员，所有包含此头文件的代码都需要重编译
};

// ============================================================================
// 4. Pimpl 与 ABI 稳定
// ============================================================================
class LibraryClass {
public:
    virtual ~LibraryClass() = default;

    // 使用 Pimpl 隐藏实现
    struct Impl;
    std::unique_ptr<Impl> pImpl_;

    LibraryClass();
    virtual void publicMethod();
};

struct LibraryClass::Impl {
    // 实现细节
    int internalState_;
    std::vector<int> buffer_;

    Impl() : internalState_(0) {}
};

LibraryClass::LibraryClass() : pImpl_(std::make_unique<Impl>()) {}
void LibraryClass::publicMethod() {
    pImpl_->internalState_++;
    cout << "LibraryClass::publicMethod, state=" << pImpl_->internalState_ << endl;
}

// ============================================================================
// 5. 工厂函数 + Pimpl
// ============================================================================
class Database {
public:
    struct Impl;
    static std::unique_ptr<Database> create(const std::string& type);

    virtual ~Database() = default;
    virtual void query(const std::string& sql) = 0;

protected:
    std::unique_ptr<Impl> pImpl_;
};

struct Database::Impl {
    std::string type_;
    Impl(const std::string& t) : type_(t) {}
};

class MySQLDatabase : public Database {
public:
    MySQLDatabase() : Database() {
        // 可以访问 pImpl_
    }
    void query(const std::string& sql) override {
        cout << "MySQL: " << sql << endl;
    }
};

class PostgreSQLDatabase : public Database {
public:
    PostgreSQLDatabase() : Database() {}
    void query(const std::string& sql) override {
        cout << "PostgreSQL: " << sql << endl;
    }
};

std::unique_ptr<Database> Database::create(const std::string& type) {
    if (type == "mysql") return std::make_unique<MySQLDatabase>();
    if (type == "postgres") return std::make_unique<PostgreSQLDatabase>();
    return nullptr;
}

// ============================================================================
// 客户端代码
// ============================================================================
int main() {
    cout << "=== Pimpl 惯用法演示 ===" << endl << endl;

    // 1. 基本 Pimpl
    cout << "1. Basic Pimpl:" << endl;
    Widget w;
    w.setData(42);
    cout << "Widget data: " << w.getData() << endl;

    // 2. Pimpl 允许修改实现而不重编译
    cout << "\n2. Implementation Hiding:" << endl;
    cout << "Impl details are hidden from header file" << endl;
    cout << "Changing Impl doesn't require recompiling users" << endl;

    // 3. ABI 稳定
    cout << "\n3. ABI Stability:" << endl;
    LibraryClass lc;
    lc.publicMethod();
    lc.publicMethod();

    // 4. 工厂 + Pimpl
    cout << "\n4. Factory + Pimpl:" << endl;
    auto db1 = Database::create("mysql");
    auto db2 = Database::create("postgres");
    if (db1) db1->query("SELECT * FROM users");
    if (db2) db2->query("SELECT * FROM users");

    // 5. Pimpl 优势总结
    cout << "\n5. Pimpl Benefits:" << endl;
    cout << "  - 编译防火墙：减少编译依赖" << endl;
    cout << "  - ABI 稳定：实现可自由变化" << endl;
    cout << "  - 头文件简洁：只暴露必要接口" << endl;
    cout << "  - 加速编译：改变实现不影响其他文件" << endl;

    return 0;
}
