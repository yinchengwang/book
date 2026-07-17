# S16 Spec —— Knowledge_Hub + Games 共享 Taro 模板

## 1. 模板结构

`engineering/apps/web/taro-template/` 目录：

```
taro-template/
├── package.json              # 公共依赖：Taro 3.6 + React 18 + Vite 5 + Tailwind
├── babel.config.js           # 公共 babel preset
├── tsconfig.json             # TypeScript strict 配置
├── postcss.config.js         # Tailwind + autoprefixer
├── tailwind.config.ts        # Tailwind theme
├── config/
│   └── index.base.ts         # Taro config 基础
└── README.md
```

## 2. Workspace 重组

`engineering/apps/web/package.json` 用 npm workspaces：

```json
{
  "name": "@book/web-workspaces",
  "workspaces": ["knowledge_hub", "games-mini-program", "taro-template"]
}
```

子项目改为 `devDependencies: { "@book/taro-template": "*" }`。

## 3. 双项目不变性

- `knowledge_hub` 与 `games-mini-program` 业务代码不变
- 仅 `dependencies` 字段瘦身（公共依赖提到 taro-template）

## 4. 不做（明确范围外）

- ❌ 强制 npm workspaces（保留各自独立运行能力）
- ❌ 升级 Taro 4.x
- ❌ 重写任一项目业务代码
- ❌ 测试 client build（仅配置抽取）
