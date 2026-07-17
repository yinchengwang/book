# S16 — 设计文档（Knowledge_Hub 共享模板抽取）

## 1. 现状

两个 Taro 项目重复配置：

```
engineering/apps/web/
├── knowledge_hub/             # Taro 3.6 学习追踪平台
│   ├── package.json          # Taro 3.6 + React 18 + dnd-kit + framer-motion + 多个
│   ├── babel.config.js
│   ├── config/index.ts
│   ├── vite.config.ts (隐含 or 没有)
│   └── ...
└── games-mini-program/       # Taro 3.6 游戏
    ├── package.json
    ├── babel.config.js
    ├── config/index.ts
    └── ...
```

**重复项**：Taro 核心依赖、React 18、TypeScript 配置、Babel 插件、Tailwind 等。

## 2. 模板结构

```
engineering/apps/web/taro-template/    # 新增
├── package.json          # Taro 3.6 + React 18 公共依赖
├── babel.config.js
├── tsconfig.json
├── postcss.config.js
├── tailwind.config.ts
├── config/
│   └── index.base.ts     # Taro config 基础
└── README.md             # 模板使用说明
```

## 3. 引用模板

每个项目保留：

```json
// 子项目 package.json
{
  "extends": "../taro-template",
  "dependencies": {
    // 仅子项目特有依赖
  }
}
```

实际上 `extends` 在 npm 中**对 package.json 不可行**——更准确做法是把 taro-template 写成 `pnpm workspace` 或 `npm workspaces` 中的共享包：

```json
// apps/web/package.json (new)
{
  "workspaces": [
    "knowledge_hub",
    "games-mini-program",
    "taro-template"
  ]
}
```

`@taro-template/core` 作为共享 npm name，子项目 `devDependencies: { "@taro-template/core": "*" }`。

## 4. 验证 V1-V4

| V | 命令 | 期望 |
|---|---|---|
| V1 | `ls engineering/apps/web/taro-template/` | 8 个文件 |
| V2 | knowledge_hub npm install | 共享依赖装入 |
| V3 | `cd knowledge_hub && npm run build:h5` | build 成功 |
| V4 | `cd games-mini-program && npm run build:h5` | build 成功 |

## 5. 不做

- ❌ 升级 Taro 到 4.x
- ❌ 删除现有重复（仅提供模板选项）
- ❌ 改 pnpm/npm workspace（保留 npm 兼容）
- ❌ 重写知识库或游戏代码
