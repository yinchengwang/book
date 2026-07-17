# operator 学习笔记

## 概念地图

C++ 运算符重载让自定义类型拥有与内建类型同等的表达力——`a + b` 可以是 Vec2 加法、复数加法、矩阵加法：

- **可重载运算符**：42 个——算术（`+ - * / %`）、比较（`== != < > <= >=`）、逻辑（`! && ||`）、位（`& | ^ ~ << >>`）、自增（`++ --`）、赋值（`= += -= *= ...`）、下标（`[]`）、调用（`()`）、箭头（`->`）、流（`<< >>`）、`new/delete`、逗号等
- **不可重载**：`.` `.*` `::` `?: ` `sizeof` `typeid` `alignof` `noexcept` 等
- **两种实现方式**：
  - **成员函数**：`Vec2 operator+(const Vec2&)` —— 左操作数是 `*this`
  - **非成员函数**（常 `friend`）：`operator+(Vec2, Vec2)` —— 两操作数对称
- **流运算符 `<<` / `>>`**：必须是非成员函数（左操作数是 `ostream`/`istream`），通常 `friend` 访问 private
- **类型转换运算符**：
  - **隐式**（C 风格）：`Vec2 → double` 编译期允许，可能意外发生
  - **`explicit`**（C++11）：阻止隐式转换，必须 `static_cast<double>(v)` 显式调用
- **运算符选择原则**：
  - 含义与内建一致：`+` 表示"加"，不能是"减"
  - 保持代数性质：`a == b` 应等价于 `b == a`、`!(a < b) && !(b < a)`
  - 不改变优先级：`a + b * c` 永远 `*` 优先

## 踩坑记录

1. **`<<` 写错为成员函数**：`ostream& operator<<(...)` 必须是成员，但 `ostream` 在标准库——必须非成员
2. **忘记返回值**：算术运算符应返回新对象（值），`+=` 应返回 `*this`（引用）
3. **`operator==` 漏写 `operator!=`**：C++20 之前需手动写两个
4. **`explicit` 误用**：单参构造加 `explicit` 防止隐式，但**聚合初始化 `Vec2{1,2}` 仍允许**
5. **`friend operator<<` 写到类内时返回 `*this` 错**：应返回 `ostream&`（引用）
6. **类型转换二义性**：`Vec2 → double` 和 `double → Vec2` 同时存在可能编译歧义
7. **`std::cin >> e` 在自动化测试中阻塞**：要避免或用 `freopen` 重定向

## 工程对照（≥100 字硬约束）

运算符重载在本仓库 `engineering/` 中体现为"代数/向量/几何"操作的 OOP 化：

1. **`engineering/src/algo-prod/quantization/quantization.c` 距离计算**：C 风格用函数 `euclidean_distance(a, b)` 和 `dot_product(a, b)`。C++ 等价物是 `Vec2 operator-(const Vec2&)` + `operator*(double)` + `double operator|(const Vec2&)`（点积用 `|` 模拟）
2. **`engineering/src/algo-prod/dict/dict_cut.c` 字符串分词**：所有 token 比较用 C 函数 `strcmp(a, b) == 0`。C++ 等价是 `std::string operator==(const std::string&)`——`std::string` 已重载
3. **`engineering/src/db/storage/vector/vector_segment.c` 向量存储**：`Vector` 结构体的距离计算 `vector_distance(v1, v2)` 是 C 函数风格——C++ 改写为 `double operator-(const Vector&)` 或成员方法 `v1.distance_to(v2)`
4. **`engineering/rag/src/rag/persist/hnsw_persist.c` HNSW 序列化**：`memcpy` 和字节级操作。C++ 改写可用 `operator<<` 重载做二进制流序列化——`std::ostream& operator<<(std::ostream&, const Vector&)`
5. **`engineering/src/algo-prod/sort/sort.c` 排序**：`qsort` + 函数指针模拟"通用排序"。C++ 直接用 `std::sort(vec.begin(), vec.end(), [](auto& a, auto& b){ return a < b; })`——运算符重载 + lambda
6. **`engineering/src/db/utils/faiss_heap.c` 优先队列**：手写堆的 `heap_push`/`heap_pop`。C++ 用 `std::priority_queue<T>` + `operator<` 重载——`pq.top()` 直接返回最小元素
7. **`engineering/src/db/index/vector_index/hnsw/hnsw_search.c` 距离比较**：手写 `min` 函数 + 数组遍历。C++ 用 `std::min_element` + 仿函数（lambda）

学完本卡能动手的事：把 `learning/scaffold/struct_union/main.c` 中的 Point 结构体重写为 C++ class + 运算符重载，对比 C 与 C++ 在数学表达上的差异。