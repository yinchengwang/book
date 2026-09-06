# Games-Mini-Program 测试环境说明

## 当前状态（2026-09-06）

- ✅ `bunx vitest run src/utils/storage.test.ts` 通过（4/4）
- ❌ `bunx vitest run`（全套件）失败：依赖链碎片化

## 问题根因

`npm install` 在本机 Windows 反复超时，导致 `node_modules` 处于半装状态：
多个 vitest 间接依赖的 CommonJS 模块只有 ESM 入口或根本没装。

涉及链路：
- `vitest` → `cross-spawn` → `which` → `isexe`（未装）
- `cross-spawn` → `path-key`（ESM-only，被 `require()`）
- `cross-spawn` → `shebang-command`（未装）
- `cross-spawn` → `semver`（未装）
- `vite-node` → `mlly` → `acorn`（未装到根 node_modules）
- `magic-string` → `@jridgewell/sourcemap-codec`（未装）

## 已打的 CJS shim

| 模块 | 位置 | 提供能力 |
|------|------|----------|
| `isexe` | `engineering/apps/web/node_modules/isexe/` | sync(path) + async(path, cb) |
| `path-key` | `games-mini-program/node_modules/vitest/node_modules/path-key/` | pathKey(options) 返回 PATH 键名 |
| `shebang-command` | `games-mini-program/node_modules/vitest/node_modules/shebang-command/` | 提取 shebang 命令名 |
| `semver` | `games-mini-program/node_modules/vitest/node_modules/semver/` | satisfies(version, range, loose) 最小实现 |
| `debug` | `engineering/apps/web/node_modules/debug/` | 返回 no-op function |

## 修复方案（推荐）

按优先级：

1. **重装依赖**（最彻底）：

   ```bash
   # 删除 lock + node_modules 后重装
   cd engineering/apps/web/games-mini-program
   rm -rf node_modules package-lock.json
   npm install --no-audit --no-fund
   ```

   Windows 上大概率再次超时。备选：WSL / 容器里跑。

2. **改用 Yarn / pnpm**（绕过 npm 碎片化）：

   ```bash
   pnpm install
   ```

   pnpm 的硬链接 + 内容寻址存储能避免重复路径下的依赖丢失。

3. **ESM 化 vitest 配置**（绕过 CJS 解析坑）：

   ```ts
   // vitest.config.ts
   export default defineConfig({
     test: {
       server: { deps: { external: ['cross-spawn'] } }
     }
   })
   ```

4. **迁出 vitest，改用 Node 24 内置 test runner**：

   ```bash
   node --test src/**/*.test.ts
   ```

   Node 18+ 自带 test runner，避开 vitest 整条工具链。

## 不要做的事

- ❌ 不要继续手动 shim 下一个缺失模块（whack-a-mole）
- ❌ 不要把 shim commit 到 git（`node_modules` 不入库）
- ❌ 不要 `npm install --force`（会引入更多冲突）

## shim 文件保留位置

为方便后续调试，5 个 shim 目前保留在 `node_modules/` 下。
下次 `npm install` 会自动覆盖。