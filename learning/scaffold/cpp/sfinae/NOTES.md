// NOTES.md — SFINAE 与 Concepts 工程对照
//
// 泛型约束机制在项目中的应用：
//
// 1. SFINAE 在项目中的使用
//    - `std::enable_if_t` 控制函数重载
//    - 避免无效模板实例化
//    - 与 std::is_xxx 配合使用
//
// 2. Concepts 的优势
//    - 代码更清晰（vs 复杂的 SFINAE）
//    - 错误信息更友好
//    - C++20 可用，但 C++17 项目仍需 SFINAE
//
// 3. 项目中的潜在应用
//    - 数据库字段类型约束
//    - 索引键类型检查
//    - 缓冲区大小验证
//
// 4. 迁移策略
//    - 现有 SFINAE 代码保持不变
//    - 新代码可考虑 Concepts
//    - 注意编译器支持情况
//
// 参考：`engineering/include/db/` 中的模板使用。
