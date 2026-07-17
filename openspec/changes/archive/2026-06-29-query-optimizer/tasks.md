# 查询优化器 - 实施计划

## 1. 项目结构与头文件

- [ ] 1.1 创建 `include/db/optimizer/optimizer.h` - 优化器公共接口
- [ ] 1.2 创建 `src/db/optimizer/CMakeLists.txt` - 构建配置
- [ ] 1.3 更新 `src/db/CMakeLists.txt` 添加 optimizer 子目录
- [ ] 1.4 更新 `test/db/CMakeLists.txt` 链接优化器库

## 2. 计划节点定义

- [ ] 2.1 定义 `plan_node_type_t` 枚举 - 节点类型
- [ ] 2.2 定义 `plan_node_t` 结构体 - 通用计划节点
- [ ] 2.3 定义 `scan_plan_t` - 扫描计划（顺序/索引）
- [ ] 2.4 定义 `join_plan_t` - 连接计划
- [ ] 2.5 定义 `project_plan_t` - 投影计划
- [ ] 2.6 定义 `filter_plan_t` - 过滤计划
- [ ] 2.7 实现 `plan_node_create()` - 创建计划节点
- [ ] 2.8 实现 `plan_node_destroy()` - 销毁计划节点

## 3. 统计信息管理

- [ ] 3.1 扩展 `table_t` 结构体添加统计信息字段
- [ ] 3.2 实现 `table_stats_update()` - 更新表统计
- [ ] 3.3 定义 `column_stats_t` 结构体 - 列统计
- [ ] 3.4 实现 `column_stats_compute()` - 计算列统计
- [ ] 3.5 实现 `analyze_table()` - ANALYZE 命令

## 4. 代价估算实现

- [ ] 4.1 定义代价常量 (seq_page_cost, random_page_cost, cpu_tuple_cost)
- [ ] 4.2 实现 `cost_seq_scan()` - 顺序扫描代价
- [ ] 4.3 实现 `cost_index_scan()` - 索引扫描代价
- [ ] 4.4 实现 `cost_hash_join()` - Hash 连接代价
- [ ] 4.5 实现 `cost_nested_loop()` - 嵌套循环代价
- [ ] 4.6 实现 `selectivity_equal()` - 等值选择性估算
- [ ] 4.7 实现 `selectivity_range()` - 范围选择性估算

## 5. 规则优化实现

- [ ] 5.1 实现 `rule_predicate_pushdown()` - 谓词下推
- [ ] 5.2 实现 `rule_constant_fold()` - 常量折叠
- [ ] 5.3 实现 `rule_column_prune()` - 列裁剪
- [ ] 5.4 实现 `rule_subquery_flatten()` - 子查询展开
- [ ] 5.5 实现 `rule_redundant_eliminate()` - 冗余消除
- [ ] 5.6 实现 `optimizer_apply_rules()` - 应用所有规则

## 6. 索引选择实现

- [ ] 6.1 实现 `index_available()` - 检测索引可用性
- [ ] 6.2 实现 `index_match_degree()` - 评估索引匹配度
- [ ] 6.3 实现 `index_cost_estimate()` - 估算索引代价
- [ ] 6.4 实现 `index_select_best()` - 选择最优索引
- [ ] 6.5 实现 `index_hint_parse()` - 解析索引提示

## 7. 优化器主入口

- [ ] 7.1 实现 `optimizer_optimize()` - 优化查询
- [ ] 7.2 实现 `optimizer_rewrite_ast()` - AST 重写
- [ ] 7.3 实现 `optimizer_physical_plan()` - 生成物理计划
- [ ] 7.4 集成到 SQL 执行器 `sql_exec.c`

## 8. EXPLAIN 实现

- [ ] 8.1 实现 `explain_plan()` - 生成计划文本
- [ ] 8.2 实现 `explain_format_tree()` - 树形格式
- [ ] 8.3 实现 `explain_format_json()` - JSON 格式
- [ ] 8.4 实现 `explain_analyze()` - 实际执行分析
- [ ] 8.5 扩展 SQL 解析器支持 EXPLAIN

## 9. 测试

- [ ] 9.1 编写规则优化测试
- [ ] 9.2 编写代价估算测试
- [ ] 9.3 编写索引选择测试
- [ ] 9.4 编写 EXPLAIN 测试
- [ ] 9.5 集成测试 - 完整查询优化流程

## 10. 文档与集成

- [ ] 10.1 更新 `build-my-db/tasks.md` 标记任务完成
- [ ] 10.2 编写优化器设计说明