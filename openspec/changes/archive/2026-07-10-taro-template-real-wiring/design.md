# S18 — Design（Taro Template Real Wiring）

## 1. 改造前 vs 改造后

### 改造前

```
apps/web/
├── knowledge_hub/         # 完整 package.json（重复 Taro deps）
└── games-mini-program/   # 完整 package.json（重复 Taro deps）
```

### 改造后

```
apps/web/
├── package.json           # 顶层 workspaces 声明
├── taro-template/         # 公共依赖 + 配置
│   ├── package.json
│   ├── babel.config.js
│   ├── tsconfig.json
│   └── config/index.base.ts
├── knowledge_hub/         # devDependencies: @book/taro-template
└── games-mini-program/   # devDependencies: @book/taro-template
```

## 2. 接入步骤

### 2.1 顶层 `apps/web/package.json`（新）

```json
{
  "name": "@book/web-workspaces",
  "version": "1.0.0",
  "private": true,
  "workspaces": [
    "taro-template",
    "knowledge_hub",
    "games-mini-program"
  ]
}
```

### 2.2 子项目 package.json 修改

**knowledge_hub/package.json**：
```diff
  "dependencies": {
    "@dnd-kit/core": "^6.1.0",
    "@dnd-kit/sortable": "^8.0.0",
-   "@tarojs/components": "3.6.40",
-   "@tarojs/mini-runner": "^3.6.37",
-   "@tarojs/plugin-framework-react": "3.6.40",
-   "@tarojs/plugin-platform-h5": "3.6.40",
-   "@tarojs/plugin-platform-weapp": "3.6.40",
-   "@tarojs/react": "3.6.40",
-   "@tarojs/runtime": "3.6.40",
-   "@tarojs/taro": "3.6.40",
-   "react": "^18.2.0",
-   "react-dom": "^18.2.0",
    "framer-motion": "^11.0.0",
    ...
  },
+ "devDependencies": {
+   "@book/taro-template": "*"
+ }
```

**games-mini-program/package.json** 同上。

### 2.3 子项目 babel.config.js

```js
// 改前：
module.exports = {
  presets: [
    ['taro', { framework: 'react', ts: true }]
  ]
};

// 改后：
module.exports = require('@book/taro-template/babel.config');
```

### 2.4 子项目 config/index.ts

```ts
// 改后：
import baseConfig from '@book/taro-template/config/index.base';
import type { IConfig } from '@tarojs/taro';

const config: IConfig = {
  ...baseConfig,
  projectName: 'reading-radar-2',
  // ... 项目特有
};

export default config;
```

## 3. 验证 V1-V4

| V | 命令 | 期望 |
|---|---|---|
| V1 | `ls apps/web/` | 含 `package.json` 顶层 |
| V2 | knowledge_hub `npm install` | 装入 `@book/taro-template` |
| V3 | knowledge_hub `npm run build:h5` | 成功 |
| V4 | games-mini-program `npm run build:h5` | 成功 |

## 4. 不做

- ❌ 升级 Taro / React 版本
- ❌ 强制迁移（双轨独立可选）
- ❌ 重写业务代码
