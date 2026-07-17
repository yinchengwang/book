# class_object 学习笔记

## 概念地图

C++ 类是 C struct 的"超集"——数据成员 + 成员函数 + 访问控制 + 继承 + 多态：

- **类的三大函数**：构造函数（创建对象）、拷贝构造函数（复制对象）、析构函数（销毁对象）。C++11 后加移动构造
- **访问控制三档**：`public`（外部可见）、`protected`（子类可见）、`private`（仅类内可见）
- **this 指针**：每个非静态成员函数隐式接收 `this` 指针指向当前对象
- **静态成员**：类级共享，定义在 `.cpp` 而非 `.h`，`Point::instances_` 形式访问
- **const 成员函数**：`double x() const`：承诺不修改，const 对象只能调 const 函数
- **友元**：`friend class PointPair;` ——友元可访问 private，破坏封装但有合法用途（如运算符重载）
- **初始化列表 vs 体内赋值**：初始化列表直接构造成员，体内赋值先默认构造再赋值。**const/引用成员必须初始化列表**

## 踩坑记录

1. **初始化顺序**：成员按声明顺序初始化，**不按初始化列表顺序**——避免在初始化列表中用未初始化成员
2. **拷贝构造忘记**：默认拷贝是浅拷贝——指针成员会双重释放。**Rule of Three/Five**
3. **`const` 成员函数修改成员**：编译错误——用 `mutable` 关键字允许修改（用于缓存/计数器）
4. **静态成员定义**：声明在类内，**定义必须在 `.cpp` 文件**——否则链接错误
5. **friend 链式传递**：友元不传递——友元的友元不是友元
6. **构造/析构函数调虚函数**：运行时类型是当前类，不是派生类——避免在构造里调虚函数

## 工程对照（≥100 字硬约束）

C++ 类与对象思想在 `engineering/` 中体现为 C 与 C++ 的范式对比：

1. **`engineering/src/db/storage/page/page.c` 的 Page 结构体**：纯 C struct，**没有成员函数**——所有操作通过 `page_read(page, ...)` 这种"以 page 为第一参数"的全局函数。这是 C 的"数据 + 算法分离"范式
2. **C++ 类的 OOP 思维**：C++ 把数据 + 算法绑到类里——`page.read(buf)` 而非 `page_read(&page, buf)`。这是"封装"理念的差异
3. **本仓库工程代码仍以 C 为主**：`grep -rn "class " engineering/src/ | wc -l` 显示 `class` 出现极少，因为工程轨用 C 实现。但 `engineering/rag/src/rag/` 部分文件用 C++ 特性（智能指针等）
4. **`engineering/include/db/catalog.h` 头文件**：`Catalog* catalog_create(void);` 等函数声明体现 C 的"不透明指针"模式——C++ 会用 `class Catalog { public: ... };` 隐藏实现
5. **C++ 类的 static 成员类比**：C 风格的全局变量 `static int instances = 0;` 在文件作用域——C++ 用 `static int instances_;` 在类内更安全（命名空间隔离）
6. **`engineering/src/db/storage/page/page.c` 的构造函数模拟**：所有 Page 必须 `page_init(page, ...)` 显式初始化——C++ 构造函数自动调，避免忘记初始化
7. **`engineering/rag/include/rag/logger.h`**：部分 RAG 模块用 C++ 类的 RAII 风格管理日志资源——C++ 优势在工程代码中逐渐显现

学完本卡能动手的事：把 `learning/scaffold/struct_union/main.c` 改写为 C++ 版本（添加构造函数/析构/const 成员函数），对比 C 与 C++ 范式差异。