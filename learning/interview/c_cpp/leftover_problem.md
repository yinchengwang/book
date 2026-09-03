[TOC]



## 一、程序除.data段之外还有哪些段，以及其存放的内容



## 二、



```c++
__attribute__((constructor))
__attribute__((destructor))
```



## 三、怎么判断引用是否占用了空间





## 四、C++的explicit修饰符

- 用于修饰类的构造函数，防止编译器进行隐式转换。
- 用于构造单参数构造函数(或有多参数但有默认参数，使得调用时只需一个参数的构造函数)

```c++
class MyClass {
public:
	explicit MyClass(int x) {
        // 构造函数实现
    }    
}
```

