# 规格：doc-nested

## ADDED Requirements

### Requirement: 嵌套文档支持

系统 SHALL 支持 JSON 嵌套文档存储和查询。

#### Scenario: 嵌套对象存储
- **WHEN** 插入嵌套文档 `{"user": {"name": "Alice", "address": {"city": "Beijing"}}}`
- **THEN** 嵌套结构 SHALL 被正确存储
- **THEN** 嵌套层次 SHALL 被保留

#### Scenario: 数组存储
- **WHEN** 插入包含数组的文档 `{"tags": ["a", "b", "c"]}`
- **THEN** 数组 SHALL 被正确存储
- **THEN** 数组索引 SHALL 可访问

#### Scenario: 混合嵌套
- **WHEN** 插入复杂嵌套文档
- **THEN** 对象和数组混合 SHALL 被支持

---

### Requirement: JSONPath 查询增强

系统 SHALL 实现增强的 JSONPath 查询。

#### Scenario: 简单路径查询
- **WHEN** 使用 `$.name`
- **THEN** 根对象的 name 字段 SHALL 被返回

#### Scenario: 嵌套路径查询
- **WHEN** 使用 `$.user.address.city`
- **THEN** 深层嵌套字段 SHALL 被返回

#### Scenario: 数组索引查询
- **WHEN** 使用 `$.tags[0]`
- **THEN** 数组第一个元素 SHALL 被返回

#### Scenario: 数组切片查询
- **WHEN** 使用 `$.tags[1:3]`
- **THEN** 数组切片 SHALL 被返回

#### Scenario: 通配符查询
- **WHEN** 使用 `$.users[*].name`
- **THEN** 所有用户的 name SHALL 被返回
- **WHEN** 使用 `$.users[?(@.age > 18)]`
- **THEN** 过滤后的用户 SHALL 被返回

---

### Requirement: 嵌套字段索引

系统 SHALL 支持嵌套字段索引。

#### Scenario: 创建嵌套字段索引
- **WHEN** 在嵌套字段上创建索引
- **WHEN** 执行 `CREATE INDEX ON documents($.user.city)`
- **THEN** 索引 SHALL 被创建
- **THEN** 嵌套字段查询 SHALL 能使用索引

#### Scenario: 多层嵌套索引
- **WHEN** 在多层嵌套字段上创建索引
- **THEN** 索引 SHALL 被正确构建
- **THEN** 查询 SHALL 高效

---

### Requirement: 嵌套文档更新

系统 SHALL 支持嵌套文档的部分更新。

#### Scenario: 更新嵌套字段
- **WHEN** 执行 `UPDATE documents SET $.user.age = 31 WHERE id = ?`
- **THEN** 只更新指定嵌套字段
- **THEN** 其他字段 SHALL 保持不变

#### Scenario: 添加嵌套字段
- **WHEN** 执行 `UPDATE documents SET $.user.phone = '123456' WHERE id = ?`
- **THEN** 新字段 SHALL 被添加
- **THEN** 现有结构 SHALL 被扩展

#### Scenario: 删除嵌套字段
- **WHEN** 执行 `UPDATE documents UNSET $.user.phone WHERE id = ?`
- **THEN** 指定字段 SHALL 被删除
