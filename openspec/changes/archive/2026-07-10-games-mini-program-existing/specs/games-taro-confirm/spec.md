# S15 Spec —— Games Mini-Program Inventory

## 1. 路径契约

- C 版本：`engineering/apps/games/{2048,snake,sudoku,mini_program}/`
- Taro 双端：`engineering/apps/web/games-mini-program/src/pages/{index,game2048,snake,sudoku,match3}/`

## 2. README 契约

- `engineering/apps/games/README.md`：双版本关系说明
- `engineering/apps/web/games-mini-program/README.md`：构建 + 双端说明

## 3. CLAUDE.md 路径

- ❌ 旧：`knowledge_hub/`
- ✅ 新：`apps/web/knowledge_hub/`

## 4. 不做（明确范围外）

- ❌ 迁移 / 重写 / 删除任何游戏
- ❌ 改 Taro 项目结构
