// NOTES.md — 模板元编程工程对照
//
// 类型系统设计在项目中的应用：
//
// 1. 类型推导
//    - `std::conditional_t` 替代分支逻辑
//    - `std::enable_if_t` 控制函数重载
//
// 2. 编译期计算
//    - 缓冲区大小可用 constexpr 计算
//    - 避免运行时计算开销
//
// 3. 项目中的元编程实践
//    - `engineering/src/db/storage/page/page.c` 用 C 实现
//    - 若迁移到 C++ 可考虑模板元编程
//    - 类型安全的参数校验
//
// 4. 权衡
//    - 编译时间增加
//    - 代码可读性降低
//    - 建议仅在性能关键路径使用
//
// 参考：`engineering/include/db/` 中的类型定义。
