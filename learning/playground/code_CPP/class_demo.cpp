#include <iostream>

class EmptyClass {};

struct EmptyStruct {};

class EmptyClass1 {
public:
    EmptyClass1() {
        // 构造函数的实现
        std::cout << "EmptyClass1 Constructor called" << std::endl;
    }

    ~EmptyClass1() {
        // 析构函数的实现
        std::cout << "EmptyClass1 Destructor called" << std::endl;
    }
};

class Base {
public:
    // 构造函数不能被声明为虚函数。这是因为构造函数的目的是初始化对象，而虚函数机制依赖于对象已经存在。
    // 虚函数表（vtable）是在对象构造过程中初始化的，而构造函数在对象完全构造之前就被调用，因此构造函数不能是虚的
    Base() {
        std::cout << "Base Constructor called" << std::endl;
    }

    virtual ~Base() {
        std::cout << "Base Destructor called" << std::endl;
    }
};

int main() {
    EmptyClass obj1;
    EmptyStruct obj2;

    // 每个对象必须有唯一的内存地址，所以编译器会为它们分配至少1个字节的空间。通常，空类和空结构体的大小是1字节。
    std::cout << "Size of EmptyClass: " << sizeof(EmptyClass) << " bytes" << std::endl;
    std::cout << "Size of EmptyStruct: " << sizeof(EmptyStruct) << " bytes" << std::endl;

    EmptyClass1 obj3;
    // 调用构造函数和析构函数只需要知道函数的地址即可，而这些函数的地址只与类型相关，而与类型的实例无关
    // 编译器也不会因为这两个函数而在实例内添加任何额外的信息
    std::cout << "Size of EmptyClass1: " << sizeof(EmptyClass1) << " bytes" << std::endl;

    Base base;
    // C++的编译器一旦发现一个类型中有虚函数，就会为该类型生成虚函数表，并在该类型的每一个实例中添加一个指向虚函数表的指针
    std::cout << "Size of Base: " << sizeof(Base) << " bytes" << std::endl;

    return 0;
}