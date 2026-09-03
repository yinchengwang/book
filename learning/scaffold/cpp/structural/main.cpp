/**
 * @file main.cpp
 * @brief 结构型设计模式演示
 *
 * 涵盖：适配器、桥接、组合、装饰器、外观、代理模式
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
// 1. 适配器模式 (Adapter)
// ============================================================================
class LegacyPrinter {
public:
    void printOld(const char* msg) { cout << "Legacy: " << msg << endl; }
};

class ModernPrinter {
public:
    virtual ~ModernPrinter() = default;
    virtual void print(const std::string& msg) const = 0;
};

class PrinterAdapter : public ModernPrinter {
public:
    explicit PrinterAdapter(LegacyPrinter& legacy) : legacy_(legacy) {}

    void print(const std::string& msg) const override {
        legacy_.printOld(msg.c_str());
    }

private:
    LegacyPrinter& legacy_;
};

// ============================================================================
// 2. 桥接模式 (Bridge)
// ============================================================================
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual void renderCircle(float x, float y, float radius) const = 0;
};

class VectorRenderer : public Renderer {
public:
    void renderCircle(float x, float y, float radius) const override {
        cout << "Vector: drawing circle at (" << x << "," << y << ") r=" << radius << endl;
    }
};

class RasterRenderer : public Renderer {
public:
    void renderCircle(float x, float y, float radius) const override {
        cout << "Raster: drawing pixels for circle at (" << x << "," << y << ") r=" << radius << endl;
    }
};

class Shape {
public:
    explicit Shape(unique_ptr<Renderer> r) : renderer_(std::move(r)) {}
    virtual ~Shape() = default;
    virtual void draw() const = 0;
    virtual void resize(float factor) = 0;

protected:
    unique_ptr<Renderer> renderer_;
};

class Circle : public Shape {
public:
    Circle(unique_ptr<Renderer> r, float x, float y, float radius)
        : Shape(std::move(r)), x_(x), y_(y), radius_(radius) {}

    void draw() const override { renderer_->renderCircle(x_, y_, radius_); }
    void resize(float factor) override { radius_ *= factor; }

private:
    float x_, y_, radius_;
};

// ============================================================================
// 3. 组合模式 (Composite)
// ============================================================================
class FileComponent {
public:
    virtual ~FileComponent() = default;
    virtual void ls(int indent = 0) const = 0;
};

class File : public FileComponent {
public:
    explicit File(std::string name) : name_(std::move(name)) {}

    void ls(int indent = 0) const override {
        cout << std::string(indent, ' ') << "File: " << name_ << endl;
    }

private:
    std::string name_;
};

class Directory : public FileComponent {
public:
    explicit Directory(std::string name) : name_(std::move(name)) {}

    void add(unique_ptr<FileComponent> component) {
        children_.push_back(std::move(component));
    }

    void ls(int indent = 0) const override {
        cout << std::string(indent, ' ') << "Dir: " << name_ << "/" << endl;
        for (const auto& child : children_) {
            child->ls(indent + 2);
        }
    }

private:
    std::string name_;
    vector<unique_ptr<FileComponent>> children_;
};

// ============================================================================
// 4. 装饰器模式 (Decorator)
// ============================================================================
class Coffee {
public:
    virtual ~Coffee() = default;
    virtual float cost() const = 0;
    virtual std::string description() const = 0;
};

class SimpleCoffee : public Coffee {
public:
    float cost() const override { return 2.0f; }
    std::string description() const override { return "Simple Coffee"; }
};

class CoffeeDecorator : public Coffee {
public:
    explicit CoffeeDecorator(unique_ptr<Coffee> c) : coffee_(std::move(c)) {}

    float cost() const override { return coffee_->cost(); }
    std::string description() const override { return coffee_->description(); }

protected:
    unique_ptr<Coffee> coffee_;
};

class MilkDecorator : public CoffeeDecorator {
public:
    using CoffeeDecorator::CoffeeDecorator;

    float cost() const override { return CoffeeDecorator::cost() + 0.5f; }
    std::string description() const override { return CoffeeDecorator::description() + ", Milk"; }
};

class SugarDecorator : public CoffeeDecorator {
public:
    using CoffeeDecorator::CoffeeDecorator;

    float cost() const override { return CoffeeDecorator::cost() + 0.2f; }
    std::string description() const override { return CoffeeDecorator::description() + ", Sugar"; }
};

// ============================================================================
// 5. 外观模式 (Facade)
// ============================================================================
class CPU {
public:
    void freeze() { cout << "CPU: Freezing..." << endl; }
    void jump(long addr) { cout << "CPU: Jumping to " << addr << endl; }
    void execute() { cout << "CPU: Executing..." << endl; }
};

class Memory {
public:
    void load(long pos, const char* data) {
        cout << "Memory: Loading at " << pos << endl;
    }
};

class Disk {
public:
    char* read(long pos, int size) {
        cout << "Disk: Reading " << size << " bytes at " << pos << endl;
        return nullptr;
    }
};

class ComputerFacade {
public:
    void start() {
        cpu_.freeze();
        memory_.load(0, "bootloader");
        cpu_.jump(0);
        cpu_.execute();
    }

private:
    CPU cpu_;
    Memory memory_;
    Disk disk_;
};

// ============================================================================
// 6. 代理模式 (Proxy)
// ============================================================================
class Image {
public:
    virtual ~Image() = default;
    virtual void display() const = 0;
};

class RealImage : public Image {
public:
    explicit RealImage(const std::string& filename) : filename_(filename) {
        loadFromDisk();
    }

    void display() const override { cout << "Displaying " << filename_ << endl; }

private:
    void loadFromDisk() const { cout << "Loading " << filename_ << " from disk..." << endl; }
    std::string filename_;
};

class ImageProxy : public Image {
public:
    explicit ImageProxy(const std::string& filename) : filename_(filename), image_(nullptr) {}

    void display() const override {
        if (!image_) {
            image_ = make_unique<RealImage>(filename_);
        }
        image_->display();
    }

private:
    mutable unique_ptr<RealImage> image_;
    std::string filename_;
};

// ============================================================================
// 客户端代码
// ============================================================================
int main() {
    cout << "=== C++ 结构型设计模式演示 ===" << endl << endl;

    // 1. 适配器
    cout << "1. Adapter:" << endl;
    LegacyPrinter legacy;
    unique_ptr<ModernPrinter> adapter = make_unique<PrinterAdapter>(legacy);
    adapter->print("Hello via adapter");

    // 2. 桥接
    cout << "\n2. Bridge:" << endl;
    auto vectorRenderer = make_unique<VectorRenderer>();
    auto circle = make_unique<Circle>(std::move(vectorRenderer), 1.0f, 2.0f, 3.0f);
    circle->draw();

    // 3. 组合
    cout << "\n3. Composite:" << endl;
    auto root = make_unique<Directory>("root");
    root->add(make_unique<File>("file1.txt"));
    auto subDir = make_unique<Directory>("subdir");
    subDir->add(make_unique<File>("file2.txt"));
    root->add(std::move(subDir));
    root->ls();

    // 4. 装饰器
    cout << "\n4. Decorator:" << endl;
    unique_ptr<Coffee> coffee = make_unique<MilkDecorator>(
        make_unique<SugarDecorator>(make_unique<SimpleCoffee>()));
    cout << coffee->description() << " = $" << coffee->cost() << endl;

    // 5. 外观
    cout << "\n5. Facade:" << endl;
    ComputerFacade computer;
    computer.start();

    // 6. 代理
    cout << "\n6. Proxy:" << endl;
    unique_ptr<Image> proxy = make_unique<ImageProxy>("photo.jpg");
    proxy->display();  // 首次加载
    proxy->display();  // 已缓存

    return 0;
}
