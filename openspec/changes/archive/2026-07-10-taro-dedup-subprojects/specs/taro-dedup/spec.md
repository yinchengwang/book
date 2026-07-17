# S25 Spec —— Taro Deps Dedup

## 1. 重复依赖清单

`taro-template/devDependencies` 已含：
- `@babel/core`
- `@tarojs/cli`
- `@types/node`
- `@types/react`
- `babel-preset-taro`
- `eslint`
- `typescript`

子项目**应移除**这些（避免重复）。

## 2. 保留子项目 devDeps

- `@playwright/test`（项目特有）
- `tailwindcss` / `postcss` / `autoprefixer`（仅 knowledge_hub）
- `react-refresh` / `@pmmmwh/react-refresh-webpack-plugin`

## 3. 不做

- ❌ 改源代码
- ❌ 改 Taro 版本
- ❌ 强制 uninstall（用户环境决定）
