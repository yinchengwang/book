# @book/taro-template

**Taro 3.6 + React 18 + TypeScript + TailwindCSS 共享模板**。

本目录为 `apps/web/` 下所有 Taro 项目提供统一配置基线：

- `knowledge_hub/` (Reading Radar 2.0 学习追踪平台)
- `games-mini-program/` (C/C++ 游戏小程序 + H5 双端)

## 文件结构

```
taro-template/
├── package.json          # 公共依赖（peerDependencies）
├── babel.config.js       # Taro babel preset 共享
├── tsconfig.json         # TypeScript strict 基线
├── config/
│   └── index.base.ts     # Taro config 基线，子项目 spread 之
└── README.md             # 本文件
```

## 子项目接入

### 1) 安装依赖

```bash
# 在子项目中
npm install --save-dev @book/taro-template
```

### 2) config 引用

```ts
// 子项目 config/index.ts
import baseConfig from '@book/taro-template/config/index.base';
const config = {
  ...baseConfig,
  projectName: 'my-app',
  pages: ['pages/index/index'],  // 项目特有
  // ... 项目特有配置
};
export default config;
```

### 3) babel 引用

```js
// 子项目 babel.config.js
module.exports = require('@book/taro-template/babel.config');
```

### 4) tsconfig 引用

```json
{
  "extends": "@book/taro-template/tsconfig",
  "compilerOptions": {
    "baseUrl": ".",
    "paths": { "@/*": ["src/*"] }
  },
  "include": ["src/**/*"]
}
```

## 公共 peerDependencies

| 包 | 版本 |
|---|---|
| @tarojs/components | 3.6.40 |
| @tarojs/react | 3.6.40 |
| @tarojs/taro | 3.6.40 |
| react | ^18.2.0 |
| react-dom | ^18.2.0 |

## 维护策略

- **template 升级**：升级 Taro / React 时同时更新 template 与两个子项目
- **子项目特有配置**：保留在子项目 `config/index.ts`
- **新增第三方公共依赖**：先加到 template，子项目 `npm install` 即可获得

## 历史

- 2026-07-10：创建（来自 S16）
- 起点：knowledge_hub + games-mini-program 的 Taro 配置 90% 重叠
