/**
 * @file main.cpp
 * @brief 行为型设计模式演示
 *
 * 涵盖：策略、观察者、命令、迭代器、模板方法、访问者模式
 * 编译：g++ -std=c++17 -o main main.cpp
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>

using std::cout;
using std::endl;
using std::make_unique;
using std::unique_ptr;
using std::vector;

// ============================================================================
// 1. 策略模式 (Strategy)
// ============================================================================
class SortStrategy {
public:
    virtual ~SortStrategy() = default;
    virtual void sort(vector<int>& data) const = 0;
};

class BubbleSort : public SortStrategy {
public:
    void sort(vector<int>& data) const override {
        cout << "BubbleSort" << endl;
        for (size_t i = 0; i < data.size(); ++i)
            for (size_t j = i + 1; j < data.size(); ++j)
                if (data[i] > data[j]) std::swap(data[i], data[j]);
    }
};

class QuickSort : public SortStrategy {
public:
    void sort(vector<int>& data) const override {
        cout << "QuickSort" << endl;
        std::sort(data.begin(), data.end());
    }
};

class Sorter {
public:
    explicit Sorter(unique_ptr<SortStrategy> strategy) : strategy_(std::move(strategy)) {}

    void setStrategy(unique_ptr<SortStrategy> strategy) { strategy_ = std::move(strategy); }

    void doSort(vector<int>& data) const { strategy_->sort(data); }

private:
    unique_ptr<SortStrategy> strategy_;
};

// ============================================================================
// 2. 观察者模式 (Observer)
// ============================================================================
class Observer {
public:
    virtual ~Observer() = default;
    virtual void update(const std::string& msg) = 0;
};

class Subject {
public:
    void attach(Observer* o) { observers_.push_back(o); }
    void detach(Observer* o) { observers_.erase(std::remove(observers_.begin(), observers_.end(), o), observers_.end()); }
    void notify(const std::string& msg) { for (auto* o : observers_) o->update(msg); }

private:
    vector<Observer*> observers_;
};

class ConcreteObserver : public Observer {
public:
    explicit ConcreteObserver(const std::string& name) : name_(name) {}
    void update(const std::string& msg) override { cout << name_ << " received: " << msg << endl; }

private:
    std::string name_;
};

// ============================================================================
// 3. 命令模式 (Command)
// ============================================================================
class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
};

class Light {
public:
    void on() { cout << "Light ON" << endl; }
    void off() { cout << "Light OFF" << endl; }
};

class LightOnCommand : public Command {
public:
    explicit LightOnCommand(Light& light) : light_(light) {}
    void execute() override { light_.on(); }

private:
    Light& light_;
};

class LightOffCommand : public Command {
public:
    explicit LightOffCommand(Light& light) : light_(light) {}
    void execute() override { light_.off(); }

private:
    Light& light_;
};

class RemoteControl {
public:
    void setCommand(unique_ptr<Command> cmd) { command_ = std::move(cmd); }
    void pressButton() { if (command_) command_->execute(); }

private:
    unique_ptr<Command> command_;
};

// ============================================================================
// 4. 迭代器模式 (Iterator)
// ============================================================================
class Iterator {
public:
    virtual ~Iterator() = default;
    virtual bool hasNext() const = 0;
    virtual int next() = 0;
};

class Aggregate {
public:
    virtual ~Aggregate() = default;
    virtual unique_ptr<Iterator> createIterator() const = 0;
    virtual void add(int val) = 0;
    virtual size_t size() const = 0;
    virtual int get(size_t idx) const = 0;
};

class ConcreteAggregate : public Aggregate {
public:
    unique_ptr<Iterator> createIterator() const override;

    void add(int val) override { data_.push_back(val); }
    size_t size() const override { return data_.size(); }
    int get(size_t idx) const override { return data_[idx]; }

private:
    vector<int> data_;
    friend class ConcreteIterator;
};

class ConcreteIterator : public Iterator {
public:
    explicit ConcreteIterator(const ConcreteAggregate& agg) : agg_(agg), index_(0) {}
    bool hasNext() const override { return index_ < agg_.size(); }
    int next() override { return agg_.get(index_++); }

private:
    const ConcreteAggregate& agg_;
    size_t index_;
};

unique_ptr<Iterator> ConcreteAggregate::createIterator() const {
    return make_unique<ConcreteIterator>(*this);
}

// ============================================================================
// 5. 模板方法模式 (Template Method)
// ============================================================================
class DataMiner {
public:
    void mine(const std::string& path) {
        openFile(path);
        extractData();
        parseData();
        analyzeData();
        sendReport();
    }

protected:
    virtual void openFile(const std::string& path) { cout << "Opening " << path << endl; }
    virtual void extractData() = 0;
    virtual void parseData() = 0;
    virtual void analyzeData() { cout << "Analyzing data..." << endl; }
    virtual void sendReport() { cout << "Sending report" << endl; }
};

class PDFDataMiner : public DataMiner {
protected:
    void extractData() override { cout << "Extracting PDF data" << endl; }
    void parseData() override { cout << "Parsing PDF structure" << endl; }
};

class CSVDataMiner : public DataMiner {
protected:
    void extractData() override { cout << "Extracting CSV data" << endl; }
    void parseData() override { cout << "Parsing CSV rows" << endl; }
    void analyzeData() override { cout << "CSV specific analysis" << endl; }
};

// ============================================================================
// 6. 访问者模式 (Visitor)
// ============================================================================
class Circle;
class Rectangle;

class ShapeVisitor {
public:
    virtual ~ShapeVisitor() = default;
    virtual void visit(const Circle& c) = 0;
    virtual void visit(const Rectangle& r) = 0;
};

class Shape {
public:
    virtual ~Shape() = default;
    virtual void accept(ShapeVisitor& v) const = 0;
};

class Circle : public Shape {
public:
    explicit Circle(double r) : radius_(r) {}
    double radius() const { return radius_; }
    void accept(ShapeVisitor& v) const override { v.visit(*this); }

private:
    double radius_;
};

class Rectangle : public Shape {
public:
    Rectangle(double w, double h) : width_(w), height_(h) {}
    double width() const { return width_; }
    double height() const { return height_; }
    void accept(ShapeVisitor& v) const override { v.visit(*this); }

private:
    double width_, height_;
};

class AreaCalculator : public ShapeVisitor {
public:
    void visit(const Circle& c) override { cout << "Circle area: " << 3.14159 * c.radius() * c.radius() << endl; }
    void visit(const Rectangle& r) override { cout << "Rectangle area: " << r.width() * r.height() << endl; }
};

class DrawVisitor : public ShapeVisitor {
public:
    void visit(const Circle& c) override { cout << "Drawing circle r=" << c.radius() << endl; }
    void visit(const Rectangle& r) override { cout << "Drawing rect " << r.width() << "x" << r.height() << endl; }
};

// ============================================================================
// 客户端代码
// ============================================================================
int main() {
    cout << "=== C++ 行为型设计模式演示 ===" << endl << endl;

    // 1. 策略
    cout << "1. Strategy:" << endl;
    vector<int> data = {5, 2, 8, 1, 9};
    Sorter sorter(make_unique<QuickSort>());
    sorter.doSort(data);
    for (int v : data) cout << v << " ";
    cout << endl;

    // 2. 观察者
    cout << "\n2. Observer:" << endl;
    Subject subject;
    ConcreteObserver o1("Observer1"), o2("Observer2");
    subject.attach(&o1);
    subject.attach(&o2);
    subject.notify("Hello!");

    // 3. 命令
    cout << "\n3. Command:" << endl;
    Light light;
    RemoteControl remote;
    remote.setCommand(make_unique<LightOnCommand>(light));
    remote.pressButton();
    remote.setCommand(make_unique<LightOffCommand>(light));
    remote.pressButton();

    // 4. 迭代器
    cout << "\n4. Iterator:" << endl;
    ConcreteAggregate agg;
    agg.add(10); agg.add(20); agg.add(30);
    auto iter = agg.createIterator();
    while (iter->hasNext()) cout << iter->next() << " ";
    cout << endl;

    // 5. 模板方法
    cout << "\n5. Template Method:" << endl;
    PDFDataMiner pdfMiner;
    pdfMiner.mine("report.pdf");

    // 6. 访问者
    cout << "\n6. Visitor:" << endl;
    vector<const Shape*> shapes;
    shapes.push_back(new Circle(5));
    shapes.push_back(new Rectangle(4, 6));
    AreaCalculator areaCalc;
    DrawVisitor drawer;
    for (const auto* s : shapes) {
        s->accept(areaCalc);
        s->accept(drawer);
    }
    for (auto* s : shapes) delete s;

    return 0;
}
