# 游戏小程序 (games-mini-program)

基于 Taro + React + TypeScript 构建的微信小程序，包含四个经典游戏：

- 🐍 **贪吃蛇** - 经典怀旧游戏
- 🔢 **数独** - 烧脑益智游戏
- 🎮 **2048** - 数字合并游戏
- 💎 **消消乐** - 匹配宝石闯关游戏

## 技术栈

- Taro 3.6.40
- React 18
- TypeScript
- SCSS
- Vitest (单元测试)
- Playwright (E2E 测试)

## 开发

```bash
# 安装依赖
cd engineering/apps/web/games-mini-program
npm install

# H5 开发模式
npm run dev:h5

# 微信小程序开发模式
npm run dev:weapp
```

## 测试

```bash
# 单元测试
npm run test:unit

# 单元测试（监听模式）
npm run test:unit:watch

# E2E 测试
npm run test:e2e

# E2E 测试（UI 模式）
npm run test:e2e:ui
```

### 测试覆盖率

运行单元测试后，覆盖率报告生成在 `coverage/` 目录：

```bash
# 查看 HTML 覆盖率报告
open coverage/index.html
```

### CI/CD

每次 Push 和 Pull Request 都会自动运行：

1. **单元测试** - 验证核心逻辑正确性
2. **E2E 测试** - 验证用户交互流程
3. **代码检查** - ESLint 代码规范
4. **构建检查** - 确保代码可正常构建

## 项目结构

```
games-mini-program/
├── src/
│   ├── pages/           # 页面
│   │   ├── index/       # 首页（游戏列表）
│   │   ├── snake/       # 贪吃蛇
│   │   ├── sudoku/      # 数独
│   │   ├── game2048/    # 2048
│   │   └── match3/      # 消消乐
│   ├── utils/           # 游戏核心逻辑
│   │   ├── snake.ts     # 贪吃蛇
│   │   ├── sudoku.ts    # 数独
│   │   ├── 2048.ts      # 2048
│   │   ├── match3.ts    # 消消乐
│   │   └── storage.ts   # 数据持久化
│   └── app.tsx          # 应用入口
├── test/                # 测试文件
│   └── unit/            # 单元测试（Vitest）
├── e2e/                 # E2E 测试（Playwright）
├── config/              # Taro 配置
└── package.json
```

## 数据持久化

游戏数据通过微信本地存储持久化：

- 最高分记录
- 关卡进度
- 星星评价

版本更新时自动迁移数据，确保用户进度不丢失。

## 与 PC 终端版的关系

本项目为**微信小程序版**，使用纯 JS 实现游戏逻辑。

PC 终端版使用 C 代码实现，位于 `engineering/apps/games/` 目录，可通过 `game_center.exe` 运行。

两个版本独立开发，互不影响。

## 贡献指南

提交代码前请确保：

1. 所有单元测试通过：`npm run test:unit`
2. 代码通过 ESLint 检查：`npm run lint`
3. 新功能添加相应的测试用例
4. 更新相关文档