# inheritance 学习笔记

## 概念地图

C++ 继承体系是 OOP 的核心机制——通过继承表达"is-a"关系，通过多态实现"运行时绑定"：

- **三种继承方式**：
  - `public` 继承 ——is-a（最常用）：父类 public → 子类 public
  - `protected` 继承：父类 public → 子类 protected（少用）
  - `private` 继承 ——has-a 实现（少见）
- **多态三要素**：
  1. **继承**：派生类继承基类
  2. **虚函数**：`virtual` 关键字标记可被覆盖的成员函数
  3. **基类指针/引用**：通过基类类型调用虚函数时，运行时绑定到实际类型
- **虚函数表 vtable**：
  - 每个含虚函数的类有一个 vtable（编译期生成）
  - 每个对象含 vptr 指针指向其类的 vtable
  - 调用虚函数：`vptr->vtable[i](this, args)`
- **`override` / `final`**（C++11）：
  - `override` 显式标记覆盖，签名错编译错误
  - `final` 阻止进一步覆盖
- **抽象类**：含 `virtual void foo() = 0;` 纯虚函数的类
- **菱形继承**：B/C 继承 A，D 多继承 B+C——需要虚基类 `virtual public A` 解决二义性

## 踩坑记录

1. **基类析构非 virtual**：`Base* p = new Derived(); delete p;` ——派生析构不调用，资源泄漏
2. **隐藏 vs 覆盖**：派生类同名函数**隐藏**基类所有同名函数（即使参数不同），用 `using Base::foo;` 引入
3. **菱形继承不虚基类**：`D` 含两份 `A` 子对象，访问二义性
4. **构造/析构调虚函数**：运行时类型是当前类，不是最终派生类
5. **`override` 误用**：基类非虚函数加 override 编译错误
6. **多继承性能**：vptr 多份、this 指针调整、虚基类开销——现代 C++ 倾向单继承 + 接口

## 工程对照（≥100 字硬约束）

C++ 继承与多态在本仓库 `engineering/` 中体现为 OOP 范式的演进：

1. **`engineering/src/db/storage/vector/vector_engine.c` 多种 vector 类型**：C 风格用函数指针表模拟"多态"——`engine->insert = my_insert; engine->search = my_search;`。这是 C 时代的"手写 vtable"
2. **C++ vtable 优势**：`grep -rn "virtual " engineering/rag/` 显示 RAG 模块用虚函数实现多态——vector_index.h 中 `class VectorIndex { virtual void insert(...); }` 派生类 HNSWIndex/IVFIndex/FlatIndex
3. **`engineering/src/db/storage/page/page.h` 的 Page 接口**：C 风格"接口"通过不透明指针 + 函数指针表实现——`Page* page = heap_page_create(); page->read(...)`。这是"用 C 模拟 C++ vtable"
4. **`engineering/include/db/storage/vector/vector_index.h`**：C++ 风格的接口抽象——`virtual VectorResult search(query) = 0;` 纯虚函数定义查询接口，多种索引实现派生
5. **派生类覆盖的实例化**：`engineering/src/db/index/vector_index/hnsw/hnsw_index.c` 实现 HNSW 版本的 search——继承 `VectorIndex` 基类，覆盖虚函数。这是 C++ OOP 在生产代码中的标准用法
6. **抽象类作为接口**：`engineering/src/db/storage/vector/vector_segment.h` 中的 `VectorSegment` 是抽象类——`virtual bool read(uint64_t id, Vector* out) = 0;`。多种 segment 实现（disk segment/mem segment）派生
7. **`engineering/src/db/consensus/raft.c` 的角色继承**：Raft 有 Candidate/Leader/Follower 三种角色——用状态机模拟而非 C++ 继承。但其 `become_candidate()` / `become_leader()` 函数模拟了"运行时类型切换"

学完本卡能动手的事：扫描 `learning/scaffold/func_pointer/main.c`（函数指针表）改写为 C++ vtable 版本，对比两者性能与可维护性差异。