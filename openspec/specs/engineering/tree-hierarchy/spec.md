# tree-hierarchy Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: 层级树查询

系统 SHALL 支持层级树数据的查询。

#### Scenario: 获取祖先节点
- **WHEN** 执行 `TREE_ANCESTORS(node_id)`
- **THEN** 从根到该节点的路径 SHALL 被返回
- **THEN** 结果 SHALL 按层级排序

#### Scenario: 获取后代节点
- **WHEN** 执行 `TREE_DESCENDANTS(node_id)`
- **THEN** 该节点的所有后代 SHALL 被返回
- **THEN** 结果 SHALL 按深度排序

#### Scenario: 获取直接子节点
- **WHEN** 执行 `TREE_CHILDREN(node_id)`
- **THEN** 直接子节点 SHALL 被返回
- **THEN** 不包含孙节点

#### Scenario: 获取父节点
- **WHEN** 执行 `TREE_PARENT(node_id)`
- **THEN** 父节点 SHALL 被返回
- **WHEN** 是根节点
- **THEN** NULL SHALL 被返回

---

### Requirement: 树深度查询

系统 SHALL 支持树深度相关查询。

#### Scenario: 获取节点深度
- **WHEN** 执行 `TREE_DEPTH(node_id)`
- **THEN** 节点深度 SHALL 被返回
- **THEN** 根节点深度为 0 或 1（可配置）

#### Scenario: 获取树高度
- **WHEN** 执行 `TREE_HEIGHT(root_id)`
- **THEN** 树的最大深度 SHALL 被返回

#### Scenario: 获取子树大小
- **WHEN** 执行 `TREE_SUBTREE_SIZE(node_id)`
- **THEN** 子树节点数量 SHALL 被返回

---

### Requirement: 树路径查询

系统 SHALL 支持树路径相关查询。

#### Scenario: 获取节点路径
- **WHEN** 执行 `TREE_PATH(node_id)`
- **THEN** 根到该节点的路径 SHALL 被返回
- **THEN** 格式为 'root/parent/.../node'

#### Scenario: 获取节点层级路径
- **WHEN** 执行 `TREE_LEVEL_PATH(node_id)`
- **THEN** 每层节点的 ID SHALL 被返回
- **THEN** 以数组形式返回

#### Scenario: 路径包含检查
- **WHEN** 检查节点是否在路径中
- **WHEN** 执行 `TREE_PATH_CONTAINS(path, node_id)`
- **THEN** 判断 SHALL 返回布尔值

---

### Requirement: XML/JSONPath 查询

系统 SHALL 支持 XML 和 JSONPath 风格的树查询。

#### Scenario: XPath 查询
- **WHEN** 使用 `/root/parent/child` 格式
- **THEN** 路径匹配 SHALL 被执行
- **THEN** 匹配节点 SHALL 被返回

#### Scenario: JSONPath 查询
- **WHEN** 使用 `$.root.parent.child` 格式
- **THEN** JSON 路径匹配 SHALL 被执行
- **THEN** 匹配节点 SHALL 被返回

#### Scenario: 通配符查询
- **WHEN** 使用 `//*` 或 `$.*`
- **THEN** 所有子节点 SHALL 被返回
- **WHEN** 使用 `/parent/*`
- **THEN** 所有直接子节点 SHALL 被返回

#### Scenario: 谓词查询
- **WHEN** 使用 `//node[@id='xxx']`
- **THEN** 条件过滤 SHALL 被应用
- **THEN** 匹配节点 SHALL 被返回

