/**
 * @file main.cpp
 * @brief SOLID 原则演示
 *
 * 涵盖：SRP/OCP/LSP/ISP/DIP 五大原则
 * 编译：g++ -std=c++17 -o main main.cpp
 */

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <memory>

using std::cout;
using std::endl;

// ============================================================================
// 1. 单一职责原则 (SRP - Single Responsibility Principle)
// ============================================================================
// 错误示例：一个类做太多事情
class UserBad {
public:
    void setName(const std::string& name) { name_ = name; }
    void save() { /* 保存到数据库 */ }
    void sendEmail() { /* 发送邮件 */ }
    void generateReport() { /* 生成报告 */ }
    // 问题：这个类有多个变化原因
private:
    std::string name_;
};

// 正确示例：每个类只有一个职责
class User {
public:
    void setName(const std::string& name) { name_ = name; }
    std::string getName() const { return name_; }

private:
    std::string name_;
};

class UserRepository {
public:
    void save(const User& user) { cout << "Saving user: " << user.getName() << endl; }
};

class EmailService {
public:
    void send(const User& user, const std::string& msg) {
        cout << "Email to " << user.getName() << ": " << msg << endl;
    }
};

class ReportGenerator {
public:
    void generate(const User& user) {
        cout << "Generating report for: " << user.getName() << endl;
    }
};

// ============================================================================
// 2. 开闭原则 (OCP - Open/Closed Principle)
// ============================================================================
// 错误示例：添加新形状需要修改 DrawShape
void drawShapeBad(int type) {
    if (type == 1) { /* 画圆 */ }
    else if (type == 2) { /* 画矩形 */ }
    // 添加新形状需要修改这段代码
}

// 正确示例：对扩展开放，对修改封闭
class Shape {
public:
    virtual ~Shape() = default;
    virtual void draw() const = 0;
};

class Circle : public Shape {
public:
    void draw() const override { cout << "Drawing Circle" << endl; }
};

class Rectangle : public Shape {
public:
    void draw() const override { cout << "Drawing Rectangle" << endl; }
};

// 添加新形状不需要修改现有代码，只需要扩展
class Triangle : public Shape {
public:
    void draw() const override { cout << "Drawing Triangle" << endl; }
};

void drawAll(const std::vector<std::unique_ptr<Shape>>& shapes) {
    for (const auto& s : shapes) s->draw();
}

// ============================================================================
// 3. 里氏替换原则 (LSP - Liskov Substitution Principle)
// ============================================================================
// 错误示例：Square 继承 Rectangle 但行为不一致
class RectangleBase {
public:
    virtual ~RectangleBase() = default;
    virtual void setWidth(double w) { width_ = w; }
    virtual void setHeight(double h) { height_ = h; }
    virtual double area() const { return width_ * height_; }

protected:
    double width_ = 0, height_ = 0;
};

class SquareBad : public RectangleBase {
public:
    void setWidth(double w) override { width_ = height_ = w; }
    void setHeight(double h) override { width_ = height_ = h; }
    // 问题：违反 LSP，正方形不是矩形的完美替代
};

// 正确示例：使用更抽象的基类
class Quadrilateral {
public:
    virtual ~Quadrilateral() = default;
    virtual double area() const = 0;
};

class RectangleGood : public Quadrilateral {
public:
    RectangleGood(double w, double h) : w_(w), h_(h) {}
    double area() const override { return w_ * h_; }

private:
    double w_, h_;
};

class SquareGood : public Quadrilateral {
public:
    explicit SquareGood(double s) : side_(s) {}
    double area() const override { return side_ * side_; }

private:
    double side_;
};

// ============================================================================
// 4. 接口隔离原则 (ISP - Interface Segregation Principle)
// ============================================================================
// 错误示例：胖接口，强迫实现不需要的方法
class MachineBad {
public:
    virtual void print() = 0;
    virtual void scan() = 0;
    virtual void fax() = 0;
};

class OldPrinter : public MachineBad {
public:
    void print() override { /* 打印 */ }
    void scan() override { /* 扫描 */ }  // 旧打印机没有扫描功能
    void fax() override { /* 传真 */ }   // 旧打印机没有传真功能
};

// 正确示例：拆分接口
class Printer {
public:
    virtual ~Printer() = default;
    virtual void print() = 0;
};

class Scanner {
public:
    virtual ~Scanner() = default;
    virtual void scan() = 0;
};

class Fax {
public:
    virtual ~Fax() = default;
    virtual void fax() = 0;
};

class OldPrinterGood : public Printer {
public:
    void print() override { cout << "Printing" << endl; }
};

class MultifunctionPrinter : public Printer, public Scanner, public Fax {
public:
    void print() override { cout << "Printing" << endl; }
    void scan() override { cout << "Scanning" << endl; }
    void fax() override { cout << "Faxing" << endl; }
};

// ============================================================================
// 5. 依赖倒置原则 (DIP - Dependency Inversion Principle)
// ============================================================================
// 错误示例：高层模块依赖低层模块
class MySQLDatabase {
public:
    void query(const std::string& sql) { cout << "MySQL: " << sql << endl; }
};

class OrderServiceBad {
public:
    void saveOrder() {
        MySQLDatabase db;  // 直接依赖具体类
        db.query("INSERT INTO orders...");
    }
};

// 正确示例：依赖抽象
class IDatabase {
public:
    virtual ~IDatabase() = default;
    virtual void query(const std::string& sql) = 0;
};

class MySQLDatabaseGood : public IDatabase {
public:
    void query(const std::string& sql) override { cout << "MySQL: " << sql << endl; }
};

class PostgreSQLDatabase : public IDatabase {
public:
    void query(const std::string& sql) override { cout << "PostgreSQL: " << sql << endl; }
};

class OrderService {
public:
    explicit OrderService(IDatabase& db) : db_(db) {}
    void saveOrder() { db_.query("INSERT INTO orders..."); }

private:
    IDatabase& db_;  // 依赖抽象
};

// ============================================================================
// 客户端代码
// ============================================================================
int main() {
    cout << "=== SOLID 原则演示 ===" << endl << endl;

    // 1. SRP
    cout << "1. SRP (Single Responsibility):" << endl;
    User user;
    user.setName("Alice");
    UserRepository repo;
    repo.save(user);
    EmailService email;
    email.send(user, "Welcome!");
    ReportGenerator report;
    report.generate(user);

    // 2. OCP
    cout << "\n2. OCP (Open/Closed):" << endl;
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<Circle>());
    shapes.push_back(std::make_unique<Rectangle>());
    shapes.push_back(std::make_unique<Triangle>());
    drawAll(shapes);

    // 3. LSP
    cout << "\n3. LSP (Liskov Substitution):" << endl;
    std::vector<std::unique_ptr<Quadrilateral>> quads;
    quads.push_back(std::make_unique<RectangleGood>(4, 5));
    quads.push_back(std::make_unique<SquareGood>(5));
    for (const auto& q : quads) {
        cout << "Area: " << q->area() << endl;
    }

    // 4. ISP
    cout << "\n4. ISP (Interface Segregation):" << endl;
    OldPrinterGood oldPrinter;
    oldPrinter.print();
    MultifunctionPrinter mfp;
    mfp.print();
    mfp.scan();
    mfp.fax();

    // 5. DIP
    cout << "\n5. DIP (Dependency Inversion):" << endl;
    MySQLDatabaseGood mysql;
    OrderService orderService(mysql);
    orderService.saveOrder();
    PostgreSQLDatabase pg;
    OrderService orderService2(pg);
    orderService2.saveOrder();

    return 0;
}
