# smart_ptr 学习笔记

## 概念地图

智能指针是 C++11 引入的"自动内存管理"工具——通过 RAII 包装裸指针，让编译器自动 delete：

- **三大智能指针**：
  - `std::unique_ptr<T>` ——独占所有权
    - 不可拷贝（构造 + 赋值 = delete）
    - 只可移动（`std::move` 转移所有权）
    - **零开销**——sizeof(unique_ptr) == sizeof(raw pointer)
  - `std::shared_ptr<T>` ——共享所有权
    - 内部含引用计数（control block）
    - 拷贝时 +1，析构/reset 时 -1，归零时 delete
    - 循环引用会**内存泄漏**——用 weak_ptr 解决
  - `std::weak_ptr<T>` ——弱引用
    - 不增加引用计数
    - 必须 `lock()` 转 shared_ptr 才能访问
    - 用于打破循环引用、缓存、观察者
- **构造方式**（C++14+）：
  - `auto p = std::make_unique<T>(args...)` ——**首选**
  - `auto p = std::make_shared<T>(args...)` ——**首选**
  - `std::unique_ptr<T> p(new T(args...))` ——次选（异常不安全）
  - `std::shared_ptr<T> p(new T(args...))` ——次选
- **自定义删除器**：
  - `unique_ptr<T, Deleter>` ——Deleter 在类型中声明
  - `shared_ptr<T>` 接受 Deleter 作为构造参数（运行时多态）
- **enable_shared_from_this**：让类的成员函数返回指向自己的 shared_ptr（不能 `shared_ptr<T>(this)`——双 ownership 灾难）

## 踩坑记录

1. **同一裸指针被多个 shared_ptr 拥有**：双 delete。**永远用 `make_shared` 或 `new` 后立刻传给 shared_ptr 构造**
2. **循环引用**：`A` shared_ptr<B> + `B` shared_ptr<A> ——引用计数永不归零。**用 weak_ptr 断环**
3. **`shared_ptr<T>(this)`**：违反所有权规则——多次 delete。**继承 `enable_shared_from_this`**
4. **`unique_ptr` 数组**：`unique_ptr<T[]>` C++14 之前需 `boost::scoped_array`
5. **`make_shared` 不支持 private 构造**：自定义 deleter 绕过
6. **`shared_ptr` 线程安全**：引用计数原子增减，但**指向的对象不是**——多线程读写对象需加锁
7. **`weak_ptr.expired()` 比 `lock()` 快**：只查 expired，不加锁

## 工程对照（≥100 字硬约束）

智能指针在 `engineering/` 中体现为 C 与 C++ 的内存管理范式对比：

1. **`engineering/src/db/storage/vector/vector_segment.c` 的 Vector 数组**：纯 C `Vector* arr = malloc(n * sizeof(Vector));` + `free(arr)`。C++ 等价物：`auto segments = std::make_unique<Vector[]>(n);`
2. **`engineering/src/db/storage/bufmgr.c` Buffer Pool 数组**：`Page* pages[MAX_PAGES] = calloc(...)` + 手动 `free`。C++ 改写：`std::vector<std::unique_ptr<Page>> pages(MAX_PAGES);`
3. **`engineering/src/db/storage/page/disk.c` 文件 fd**：`int fd = open(path, O_RDONLY); ... close(fd);` ——手动 close。C++：`std::unique_ptr<FILE, FileCloser> fp(fopen(...));`
4. **`engineering/rag/src/rag/persist/hnsw_persist.c` 多层资源**：HNSW 图节点 `HNSWNode* nodes = malloc(...); ... free(nodes);`——三层资源都要手动释放。C++：`std::vector<std::shared_ptr<HNSWNode>> nodes;` 自动管理
5. **`engineering/src/db/index/vector_index/hnsw/hnsw_search.c` 搜索结果**：`SearchResult* result = malloc(...); ... free(result);`——返回后手动 free 易遗漏。C++：`std::unique_ptr<SearchResult> result = std::make_unique<SearchResult>();`
6. **`engineering/src/db/consensus/raft.c` 节点引用**：Raft peer 节点相互引用——C 风格用指针数组手动管理。C++ 改写：`std::shared_ptr<RaftNode>` + `std::weak_ptr<RaftNode>` 防循环
7. **`engineering/src/db/api/handlers.c` HTTP handler**：C 风格 `void* ctx` 携带。C++ 用 `std::shared_ptr<RequestContext>` 自动管理生命周期

学完本卡能动手的事：把 `learning/scaffold/dynamic_memory/main.c` 的 malloc/free 改写为 C++ `std::unique_ptr`/`std::shared_ptr`，观察代码量减少与内存安全提升。