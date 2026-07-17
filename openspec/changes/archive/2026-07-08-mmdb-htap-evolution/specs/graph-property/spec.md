# 规格：graph-property

## ADDED Requirements

### Requirement: 属性图模型

系统 SHALL 实现属性图数据模型。

#### Scenario: 顶点属性
- **WHEN** 创建顶点
- **WHEN** 设置属性（如 name, age）
- **THEN** 属性 SHALL 被存储
- **THEN** 属性类型 SHALL 被记录

#### Scenario: 边属性
- **WHEN** 创建边
- **WHEN** 设置属性（如 weight, since）
- **THEN** 属性 SHALL 被存储
- **THEN** 属性类型 SHALL 被记录

#### Scenario: 标签系统
- **WHEN** 给顶点/边添加标签
- **THEN** 顶点可以有一个或多个标签
- **THEN** 边可以有一个或多个标签

---

### Requirement: 图 Schema

系统 SHALL 实现图 Schema 定义。

#### Scenario: 定义顶点类型
- **WHEN** 执行 `CREATE VERTEX LABEL Person (name VARCHAR, age INT)`
- **THEN** 顶点 Schema SHALL 被定义
- **THEN** 属性类型 SHALL 被记录

#### Scenario: 定义边类型
- **WHEN** 执行 `CREATE EDGE LABEL KNOWS (since TIMESTAMP)`
- **THEN** 边 Schema SHALL 被定义
- **THEN** 属性类型 SHALL 被记录

#### Scenario: Schema 验证
- **WHEN** 插入不符合 Schema 的数据
- **THEN** 错误 SHALL 被报告
- **THEN** 验证失败消息 SHALL 被返回

---

### Requirement: 顶点操作

系统 SHALL 实现顶点 CRUD 操作。

#### Scenario: 创建顶点
- **WHEN** 执行 `INSERT INTO Person (name, age) VALUES ('Alice', 30)`
- **THEN** 顶点 SHALL 被创建
- **THEN** 顶点 ID SHALL 被返回

#### Scenario: 读取顶点
- **WHEN** 执行 `SELECT * FROM Person WHERE id = ?`
- **THEN** 顶点属性 SHALL 被返回

#### Scenario: 更新顶点
- **WHEN** 执行 `UPDATE Person SET age = 31 WHERE id = ?`
- **THEN** 顶点属性 SHALL 被更新

#### Scenario: 删除顶点
- **WHEN** 执行 `DELETE FROM Person WHERE id = ?`
- **THEN** 顶点 SHALL 被删除
- **THEN** 关联边 SHALL 被处理（级联或阻止）

---

### Requirement: 边操作

系统 SHALL 实现边 CRUD 操作。

#### Scenario: 创建边
- **WHEN** 执行 `INSERT INTO KNOWS (src, dst, since) VALUES (?, ?, ?)`
- **THEN** 边 SHALL 被创建
- **THEN** 源顶点和目标顶点 SHALL 被关联

#### Scenario: 读取边
- **WHEN** 执行 `SELECT * FROM KNOWS WHERE src = ?`
- **THEN** 边的属性 SHALL 被返回

#### Scenario: 更新边
- **WHEN** 执行 `UPDATE KNOWS SET since = ? WHERE src = ? AND dst = ?`
- **THEN** 边属性 SHALL 被更新

#### Scenario: 删除边
- **WHEN** 执行 `DELETE FROM KNOWS WHERE src = ? AND dst = ?`
- **THEN** 边 SHALL 被删除

---

### Requirement: 图索引

系统 SHALL 实现图索引。

#### Scenario: 标签索引
- **WHEN** 查询特定标签的顶点
- **THEN** 标签索引 SHALL 被使用
- **THEN** 扫描范围 SHALL 被减少

#### Scenario: 属性索引
- **WHEN** 按属性值查询
- **THEN** 属性索引 SHALL 被使用
- **THEN** 查找效率 SHALL 被提高

#### Scenario: 复合索引
- **WHEN** 按多个属性查询
- **THEN** 复合索引 SHALL 被使用
- **THEN** 索引 SHALL 被正确组合
