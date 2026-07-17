# S25 — Design（Taro Deps Dedup）

## 1. taro-template 含有的 devDeps

```
@tarojs/cli, @babel/core, @types/node, @types/react,
babel-preset-taro, eslint, typescript
```

## 2. 子项目删的 deps

knowledge_hub / games-mini-program 中可删：
- `@tarojs/cli` ✅ 在 template
- `@tarojs/webpack-runner` ✅ 在 template
- `babel-preset-taro` ✅ 在 template
- `@types/node` ✅ 在 template
- `@types/react` ✅ 在 template
- `eslint` ✅ 在 template
- `eslint-config-taro`（仅 knowledge_hub）— 留作项目特有
- `eslint-plugin-react`（仅 knowledge_hub）— 留作项目特有
- `typescript` ✅ 在 template

## 3. 不删

子项目特有：
- `@playwright/test`
- `@pmmmwh/react-refresh-webpack-plugin`
- `tailwindcss`, `postcss`, `autoprefixer`（knowledge_hub）
- `react-refresh`

## 4. 验证

| V | 命令 |
|---|---|
| V1 | `npm ls --workspaces` 看到模板被引用 |
| V2 | knowledge_hub devDep 减至 ~10 个 |
| V3 | games-mini-program devDep 减至 ~8 个 |
