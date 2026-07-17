# S10 Spec —— OpenSpec 规格组织

## 1. 分类规则

`openspec/specs/<capability>/spec.md` 必须归属下列子目录之一：

- `engineering/` —— 描述 `engineering/src/<子系统>/<模块>/` 或 `engineering/apps/<模块>/` 的能力规格
- `learning/` —— 描述 `learning/notes/<主题>/`、`learning/code-solutions/<模块>/`、`apps/web/knowledge_hub/<功能>/` 等学习相关规格
- `archived/` —— 已废弃规格

## 2. spec.md 格式约束

- 必须是 OpenSpec 声明式（Purpose / Requirements / Scenarios）
- 单个 spec.md 不少于 30 行（少于视为空泛，应通过 change 流程丰富）

## 3. 添加流程

新规格 → `openspec/changes/<change>/specs/<capability>/spec.md` → 归档时移到 `openspec/specs/{engineering,learning}/<capability>/spec.md`

## 4. 删除流程

已废弃规格：
1. 在 change/archive 中标记为 REMOVED
2. `git rm -r openspec/specs/<capability>/`
3. README.md 添加 ARCHIVED 段说明删除原因
