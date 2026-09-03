# inheritance scaffold

C++ 继承与多态演示——单继承/vtable/override/final/抽象类/多继承。

## 复现命令

```bash
cd learning/scaffold/inheritance

g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
```

## 关键点

- **`public` 继承**：is-a 关系，public 成员在派生类仍是 public
- **虚函数表（vtable）**：每个含虚函数的类有一个 vtable，对象含 vptr 指向 vtable。多态通过 vptr 间接调用
- **`override`**（C++11）：显式标记覆盖，签名错时编译器报错
- **`final`**：阻止类进一步继承 / 阻止虚函数进一步覆盖
- **抽象类**：含纯虚函数 `= 0` 的类，**不能实例化**，只能作为基类
- **多继承**：可同时继承多个类（C++ 支持）；菱形继承需要**虚基类**避免二义性
- **基类析构必须 virtual**：否则通过基类指针删除派生对象时，派生析构不调用 → 内存泄漏

详见 NOTES.md 工程对照段。