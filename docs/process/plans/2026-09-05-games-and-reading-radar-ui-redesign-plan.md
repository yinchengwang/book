# Games & Reading Radar UI Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign and modernize the UI of two web projects (games web + reading-radar) using React + TypeScript + Vite, preserving the C → WASM game core, with industry best practices (animations, undo, themes, mobile support).

**Architecture:** Two projects migrated in parallel via MVP phases. Each MVP has a demo-able deliverable. Frontend (React+TS) talks to C game core compiled to WASM via typed bindings. Shared design system across both projects.

**Tech Stack:** React 18, TypeScript 5, Vite 5, TailwindCSS 3.4, Zustand, React Router 6, Framer Motion, react-i18next, Vitest, Playwright, Emscripten 6.0.9 (emcc)

**Spec:** `D:\code\book\docs\superpowers\specs\2026-09-05-games-and-reading-radar-ui-redesign-design.md`

## Global Constraints

- TypeScript strict mode enabled (no `any`, no implicit returns)
- All file paths absolute: `D:\code\book\engineering\apps\games\web\...` 或 `D:\code\book\engineering\apps\web\reading-radar\...`
- WASM build via emcc 6.0.9 at `D:/code/book/emsdk/upstream/emscripten/emcc.exe`
- 旧 HTML 文件过渡期保留，加"新版上线"横幅
- 所有改动通过 git commit（conventional commits: feat/fix/refactor/chore）
- 任何任务结束都必须能独立演示

---

## Phase 概览

| MVP | 时间 | 交付物 | 路径 |
|-----|------|--------|------|
| **MVP-1** | W1-W2 | 设计系统 + WASM 重构 + 2048 完整迁移 | `games/web` |
| **MVP-2** | W3-W4 | 贪吃蛇 + 数独 Web 版 | `games/web` |
| **MVP-3** | W5-W7 | 游戏 web 收尾（暗色、动画、撤销、笔记、提示、成就） | `games/web` |
| **MVP-4** | W8-W9 | 读书雷达基础设施 + 数据层 | `reading-radar` |
| **MVP-5** | W10-W12 | Quiz / Learn / Kanban / Dashboard 迁移 | `reading-radar` |
| **MVP-6** | W13-W14 | 次要页 + 测试 + 文档 | `reading-radar` |

---

# MVP-1: Foundation + WASM Rebuild + 2048 (W1-W2)

> **Demo 能力**：可玩的现代化 2048 + 统一设计系统已就位 + WASM 绑定稳定。

## Task 1.1: 项目基础设施搭建

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\package.json`
- Create: `D:\code\book\engineering\apps\games\web\vite.config.ts`
- Create: `D:\code\book\engineering\apps\games\web\tsconfig.json`
- Create: `D:\code\book\engineering\apps\games\web\tailwind.config.ts`
- Create: `D:\code\book\engineering\apps\games\web\postcss.config.js`
- Create: `D:\code\book\engineering\apps\games\web\.gitignore`

**Interfaces:**
- Consumes: 现有 `D:\code\book\emsdk\upstream\emscripten\emcc.exe`
- Produces: 可用 `npm run dev` 启动的 Vite 项目

- [ ] **Step 1.1.1: 创建 package.json**

```json
{
  "name": "games-web",
  "private": true,
  "version": "2.0.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "tsc -b && vite build",
    "preview": "vite preview",
    "wasm:build": "bash wasm-src/build.sh",
    "test": "vitest",
    "test:e2e": "playwright test"
  },
  "dependencies": {
    "react": "^18.2.0",
    "react-dom": "^18.2.0",
    "react-router-dom": "^6.20.0",
    "zustand": "^4.5.0",
    "framer-motion": "^11.0.0",
    "react-i18next": "^14.0.0",
    "i18next": "^23.7.0",
    "lucide-react": "^0.300.0",
    "clsx": "^2.0.0",
    "tailwind-merge": "^2.0.0"
  },
  "devDependencies": {
    "@types/react": "^18.2.0",
    "@types/react-dom": "^18.2.0",
    "@vitejs/plugin-react": "^4.2.0",
    "typescript": "^5.3.0",
    "vite": "^5.0.0",
    "tailwindcss": "^3.4.0",
    "postcss": "^8.4.32",
    "autoprefixer": "^10.4.16",
    "vitest": "^1.0.0",
    "@playwright/test": "^1.40.0"
  }
}
```

- [ ] **Step 1.1.2: 创建 vite.config.ts**

```ts
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
      '@shared': path.resolve(__dirname, '../shared/web/src'), // 跨项目共享
    },
  },
  server: {
    port: 5173,
    headers: {
      'Cross-Origin-Opener-Policy': 'same-origin',
      'Cross-Origin-Embedder-Policy': 'require-corp',
    },
  },
  build: {
    rollupOptions: {
      output: {
        manualChunks: {
          'g2048': ['./src/games/g2048'],
          'snake': ['./src/games/snake'],
          'sudoku': ['./src/games/sudoku'],
        },
      },
    },
  },
});
```

- [ ] **Step 1.1.3: 创建 tsconfig.json**

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2022", "DOM", "DOM.Iterable", "WebWorker"],
    "module": "ESNext",
    "moduleResolution": "bundler",
    "allowImportingTsExtensions": false,
    "resolveJsonModule": true,
    "isolatedModules": true,
    "noEmit": true,
    "jsx": "react-jsx",
    "strict": true,
    "noUnusedLocals": true,
    "noUnusedParameters": true,
    "noFallthroughCasesInSwitch": true,
    "skipLibCheck": true,
    "esModuleInterop": true,
    "allowSyntheticDefaultImports": true,
    "baseUrl": ".",
    "paths": {
      "@/*": ["src/*"],
      "@shared/*": ["../shared/web/src/*"]
    }
  },
  "include": ["src", "tests"],
  "references": []
}
```

- [ ] **Step 1.1.4: 创建 tailwind.config.ts**

```ts
import type { Config } from 'tailwindcss';

export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  darkMode: 'class',
  theme: {
    extend: {
      colors: {
        primary: {
          50: '#eef2ff', 500: '#6366f1', 900: '#312e81',
        },
        g2048: {
          bg: '#faf8ef',
          tile: {
            2: '#eee4da', 4: '#ede0c8', 8: '#f2b179', 16: '#f59563',
            32: '#f67c5f', 64: '#f65e3b', 128: '#edcf72', 256: '#edcc61',
            512: '#edc850', 1024: '#edc53f', 2048: '#edc22e',
          },
        },
        snake: { board: '#f5f5f5', snake: '#2ecc71', food: '#e74c3c' },
        sudoku: { grid: '#bbada0', cell: '#faf8ef', accent: '#3b82f6' },
      },
      fontFamily: {
        sans: ['Inter', 'Noto Sans SC', 'system-ui', 'sans-serif'],
        mono: ['JetBrains Mono', 'Cascadia Code', 'monospace'],
      },
    },
  },
  plugins: [],
} satisfies Config;
```

- [ ] **Step 1.1.5: 创建 postcss.config.js**

```js
export default {
  plugins: { tailwindcss: {}, autoprefixer: {} },
};
```

- [ ] **Step 1.1.6: 创建 .gitignore**

```
node_modules/
dist/
.wasm-cache/
*.log
.env
.env.local
playwright-report/
test-results/
```

- [ ] **Step 1.1.7: 安装依赖**

Run: `cd "D:\code\book\engineering\apps\games\web" && npm install`
Expected: 依赖安装成功，无错误

- [ ] **Step 1.1.8: 验证可启动**

Run: `cd "D:\code\book\engineering\apps\games\web" && npm run dev -- --help`
Expected: Vite 命令帮助显示

- [ ] **Step 1.1.9: 提交**

```bash
cd "D:\code\book" && git add engineering/apps/games/web/package.json engineering/apps/games/web/vite.config.ts engineering/apps/games/web/tsconfig.json engineering/apps/games/web/tailwind.config.ts engineering/apps/games/web/postcss.config.js engineering/apps/games/web/.gitignore
git commit -m "chore(games-web): scaffold vite + react + ts project"
```

---

## Task 1.2: 设计系统基础设施

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\src\main.tsx`
- Create: `D:\code\book\engineering\apps\games\web\src\App.tsx`
- Create: `D:\code\book\engineering\apps\games\web\src\index.css`
- Create: `D:\code\book\engineering\apps\games\web\shared\web\src\theme\tokens.ts`
- Create: `D:\code\book\engineering\apps\games\web\shared\web\src\theme\ThemeProvider.tsx`
- Create: `D:\code\book\engineering\apps\games\web\shared\web\src\ui\Button.tsx`
- Create: `D:\code\book\engineering\apps\games\web\index.html`

**Interfaces:**
- Consumes: Vite 项目脚手架（Task 1.1）
- Produces: `<Button />`、`<ThemeProvider />`、`tokens` 对象

- [ ] **Step 1.2.1: 提取共享 tokens**

```ts
// shared/web/src/theme/tokens.ts
export const tokens = {
  color: {
    primary: { 50: '#eef2ff', 500: '#6366f1', 900: '#312e81' },
    g2048: {
      bg: '#faf8ef',
      tile: { 2: '#eee4da', 4: '#ede0c8', 8: '#f2b179', 16: '#f59563',
             32: '#f67c5f', 64: '#f65e3b', 128: '#edcf72', 256: '#edcc61',
             512: '#edc850', 1024: '#edc53f', 2048: '#edc22e' },
    },
    snake: { board: '#f5f5f5', snake: '#2ecc71', food: '#e74c3c' },
    sudoku: { grid: '#bbada0', cell: '#faf8ef', accent: '#3b82f6' },
  },
  spacing: { 1: '4px', 2: '8px', 3: '12px', 4: '16px', 8: '32px' },
  radius: { sm: '4px', md: '8px', lg: '16px', full: '9999px' },
  shadow: { sm: '0 1px 2px rgba(0,0,0,.05)', md: '0 4px 6px rgba(0,0,0,.1)' },
} as const;
```

- [ ] **Step 1.2.2: 创建 ThemeProvider**

```tsx
// shared/web/src/theme/ThemeProvider.tsx
import { createContext, useContext, useEffect, useState, ReactNode } from 'react';

type Theme = 'light' | 'dark';

interface ThemeContextValue {
  theme: Theme;
  toggle: () => void;
  setTheme: (t: Theme) => void;
}

const ThemeContext = createContext<ThemeContextValue | null>(null);

export function ThemeProvider({ children }: { children: ReactNode }) {
  const [theme, setTheme] = useState<Theme>(() => {
    const saved = localStorage.getItem('theme');
    if (saved === 'light' || saved === 'dark') return saved;
    return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
  });

  useEffect(() => {
    document.documentElement.classList.toggle('dark', theme === 'dark');
    localStorage.setItem('theme', theme);
  }, [theme]);

  return (
    <ThemeContext.Provider value={{
      theme, setTheme,
      toggle: () => setTheme(t => t === 'light' ? 'dark' : 'light'),
    }}>
      {children}
    </ThemeContext.Provider>
  );
}

export function useTheme() {
  const ctx = useContext(ThemeContext);
  if (!ctx) throw new Error('useTheme must be inside ThemeProvider');
  return ctx;
}
```

- [ ] **Step 1.2.3: 创建 Button 组件**

```tsx
// shared/web/src/ui/Button.tsx
import { ButtonHTMLAttributes, forwardRef } from 'react';
import { clsx } from 'clsx';

type Variant = 'primary' | 'ghost' | 'icon';
type Size = 'sm' | 'md' | 'lg';

interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: Variant;
  size?: Size;
}

export const Button = forwardRef<HTMLButtonElement, ButtonProps>(
  ({ variant = 'primary', size = 'md', className, ...rest }, ref) => {
    return (
      <button
        ref={ref}
        className={clsx(
          'inline-flex items-center justify-center rounded-md transition-colors',
          'focus:outline-none focus:ring-2 focus:ring-primary-500',
          size === 'sm' && 'px-2 py-1 text-sm',
          size === 'md' && 'px-4 py-2',
          size === 'lg' && 'px-6 py-3 text-lg',
          variant === 'primary' && 'bg-primary-500 text-white hover:bg-primary-600',
          variant === 'ghost' && 'bg-transparent hover:bg-gray-100 dark:hover:bg-gray-800',
          variant === 'icon' && 'p-2 bg-transparent hover:bg-gray-100 dark:hover:bg-gray-800',
          className
        )}
        {...rest}
      />
    );
  }
);
Button.displayName = 'Button';
```

- [ ] **Step 1.2.4: 创建 index.css**

```css
/* src/index.css */
@tailwind base;
@tailwind components;
@tailwind utilities;

@layer base {
  html { @apply antialiased; }
  body { @apply bg-white text-gray-900 dark:bg-gray-900 dark:text-gray-100; }
}
```

- [ ] **Step 1.2.5: 创建 index.html 入口**

```html
<!DOCTYPE html>
<html lang="zh-CN">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>游戏中心</title>
</head>
<body class="h-full">
  <div id="root" class="h-full"></div>
  <script type="module" src="/src/main.tsx"></script>
</body>
</html>
```

- [ ] **Step 1.2.6: 创建 main.tsx 入口**

```tsx
// src/main.tsx
import React from 'react';
import ReactDOM from 'react-dom/client';
import { App } from './App';
import { ThemeProvider } from '@shared/theme/ThemeProvider';
import './index.css';

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <ThemeProvider>
      <App />
    </ThemeProvider>
  </React.StrictMode>
);
```

- [ ] **Step 1.2.7: 创建占位 App.tsx**

```tsx
// src/App.tsx
import { Button } from '@shared/ui/Button';
import { useTheme } from '@shared/theme/ThemeProvider';

export function App() {
  const { theme, toggle } = useTheme();
  return (
    <div className="min-h-screen flex flex-col items-center justify-center gap-4">
      <h1 className="text-4xl font-bold">🎮 游戏中心</h1>
      <Button onClick={toggle}>切换到{theme === 'light' ? '暗色' : '亮色'}</Button>
    </div>
  );
}
```

- [ ] **Step 1.2.8: 启动验证**

Run: `cd "D:\code\book\engineering\apps\games\web" && npm run dev`
Expected: 浏览器打开 http://localhost:5173 看到 "🎮 游戏中心" 和切换按钮

- [ ] **Step 1.2.9: 提交**

```bash
git add engineering/apps/games/web/src engineering/apps/games/web/shared engineering/apps/games/web/index.html
git commit -m "feat(games-web): setup design system with tokens, theme provider, button"
```

---

## Task 1.3: WASM 绑定重构（解决命名冲突）

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\wasm-src\binding.c`
- Create: `D:\code\book\engineering\apps\games\web\wasm-src\g2048.h`
- Create: `D:\code\book\engineering\apps\games\web\wasm-src\g2048.c`
- Create: `D:\code\book\engineering\apps\games\web\wasm-src\build.sh`

**Interfaces:**
- Consumes: 现有 `engineering\apps\games\core\g2048_core.{c,h}` 和 `engineering\apps\games\core\snake_core.{c,h}`
- Produces: 新的 `public/wasm/games.js` 和 `games.wasm`，无符号冲突

- [ ] **Step 1.3.1: 创建 wasm-src/g2048.h（API 头）**

```c
// wasm-src/g2048.h
#ifndef G2048_H
#define G2048_H

#include <stdbool.h>
#define G2048_SIZE 4

typedef enum { G2048_UP, G2048_DOWN, G2048_LEFT, G2048_RIGHT } G2048Dir;

typedef struct {
  int board[G2048_SIZE][G2048_SIZE];
  int score;
  bool game_over, won;
} G2048Game;

void g2048_init(G2048Game *g, int seed);
int  g2048_move(G2048Game *g, G2048Dir dir);
int  g2048_tile_at(const G2048Game *g, int row, int col);
bool g2048_can_move(const G2048Game *g);

#endif
```

- [ ] **Step 1.3.2: 创建 wasm-src/g2048.c（最小实现）**

从 `engineering\apps\games\core\g2048_core.c` 复制内容，更新类型定义匹配新的 `G2048Game` 结构（不含 `moved`、`keep_going` 字段以减少 WASM 体积）。

- [ ] **Step 1.3.3: 创建 binding.c（统一导出层）**

```c
// wasm-src/binding.c
#include <emscripten/emscripten.h>
#include "g2048.h"

static G2048Game g_state;

EMSCRIPTEN_KEEPALIVE
void g2048_init_js(int seed) {
  g2048_init(&g_state, seed);
}

EMSCRIPTEN_KEEPALIVE
int g2048_move_js(int dir) {
  return g2048_move(&g_state, (G2048Dir)dir);
}

EMSCRIPTEN_KEEPALIVE
int g2048_tile_js(int row, int col) {
  return g2048_tile_at(&g_state, row, col);
}

EMSCRIPTEN_KEEPALIVE
int g2048_score_js(void) {
  return g_state.score;
}

EMSCRIPTEN_KEEPALIVE
int g2048_game_over_js(void) {
  return g_state.game_over ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int g2048_won_js(void) {
  return g_state.won ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int g2048_can_move_js(void) {
  return g2048_can_move(&g_state) ? 1 : 0;
}
```

- [ ] **Step 1.3.4: 创建 build.sh**

```bash
#!/bin/bash
# wasm-src/build.sh
set -e
EMCC="D:/code/book/emsdk/upstream/emscripten/emcc.exe"
PROJECT_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WASM_SRC="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="$PROJECT_ROOT/public/wasm"

mkdir -p "$OUT_DIR"

"$EMCC" -o "$OUT_DIR/games.js" \
  "$WASM_SRC/binding.c" \
  "$WASM_SRC/g2048.c" \
  -lm \
  -s MODULARIZE=1 \
  -s EXPORT_NAME="GameModule" \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORTED_FUNCTIONS="['_g2048_init_js','_g2048_move_js','_g2048_tile_js','_g2048_score_js','_g2048_game_over_js','_g2048_won_js','_g2048_can_move_js']" \
  -O3

echo "✅ WASM built at $OUT_DIR"
```

- [ ] **Step 1.3.5: 编译验证**

Run: `cd "D:\code\book\engineering\apps\games\web" && bash wasm-src/build.sh`
Expected: 输出 "✅ WASM built at..."  无符号错误

- [ ] **Step 1.3.6: 验证生成文件**

Run: `ls -la "D:\code\book\engineering\apps\games\web\public\wasm"`
Expected: 看到 `games.js` 和 `games.wasm` 两个文件

- [ ] **Step 1.3.7: 提交**

```bash
git add engineering/apps/games/web/wasm-src engineering/apps/games/web/public
git commit -m "feat(games-web): rebuild wasm bindings with namespace prefix"
```

---

## Task 1.4: WASM 加载器 + TypeScript 绑定

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\src\wasm\types.ts`
- Create: `D:\code\book\engineering\apps\games\web\src\wasm\loader.ts`
- Create: `D:\code\book\engineering\apps\games\web\src\wasm\bindings.ts`

**Interfaces:**
- Consumes: `public/wasm/games.js` 暴露的 `window.GameModule()`
- Produces: `loadWasm(): Promise<GameExports>` + 类型安全的 `game2048` 对象

- [ ] **Step 1.4.1: 创建 WASM 类型定义**

```ts
// src/wasm/types.ts
export interface G2048Exports {
  _g2048_init_js(seed: number): void;
  _g2048_move_js(dir: number): number;
  _g2048_tile_js(row: number, col: number): number;
  _g2048_score_js(): number;
  _g2048_game_over_js(): number;
  _g2048_won_js(): number;
  _g2048_can_move_js(): number;
}

export interface SnakeExports {
  _snake_init_js(seed: number, difficulty: number): void;
  _snake_tick_js(): void;
  _snake_input_js(dir: number): void;
  _snake_len_js(): number;
  _snake_body_x_js(i: number): number;
  _snake_body_y_js(i: number): number;
  _snake_food_x_js(): number;
  _snake_food_y_js(): number;
  _snake_score_js(): number;
  _snake_over_js(): number;
}

export interface GameExports extends G2048Exports, SnakeExports {}

interface GameModuleFactory {
  (options?: { locateFile?: (path: string) => string }): Promise<GameExports>;
}

declare global {
  interface Window {
    GameModule?: GameModuleFactory;
  }
}
```

- [ ] **Step 1.4.2: 实现 loader**

```ts
// src/wasm/loader.ts
import type { GameExports } from './types';

let modulePromise: Promise<GameExports> | null = null;

export interface LoadOptions {
  wasmPath?: string;
  onProgress?: (loaded: number, total: number) => void;
}

export async function loadWasm(opts: LoadOptions = {}): Promise<GameExports> {
  if (modulePromise) return modulePromise;

  modulePromise = (async () => {
    if (typeof window === 'undefined' || !window.GameModule) {
      throw new Error(
        '[wasm] GameModule 未找到。请确认 public/wasm/games.js 已生成。' +
        '运行: npm run wasm:build'
      );
    }

    const module = await window.GameModule({
      locateFile: (path) => opts.wasmPath ? `${opts.wasmPath}/${path}` : path,
    });

    return module as unknown as GameExports;
  })();

  return modulePromise;
}

export function resetWasmCache() {
  modulePromise = null;
}
```

- [ ] **Step 1.4.3: 实现 bindings 包装层**

```ts
// src/wasm/bindings.ts
import { loadWasm } from './loader';
import type { GameExports } from './types';

export const g2048 = {
  async init(seed: number) {
    const m = await loadWasm();
    m._g2048_init_js(seed);
  },
  async move(dir: 0 | 1 | 2 | 3): Promise<boolean> {
    const m = await loadWasm();
    return m._g2048_move_js(dir) === 1;
  },
  async tile(row: number, col: number): Promise<number> {
    const m = await loadWasm();
    if (row < 0 || row > 3 || col < 0 || col > 3) {
      throw new RangeError(`Invalid cell: ${row},${col}`);
    }
    return m._g2048_tile_js(row, col);
  },
  async score(): Promise<number> {
    const m = await loadWasm();
    return m._g2048_score_js();
  },
  async gameOver(): Promise<boolean> {
    const m = await loadWasm();
    return m._g2048_game_over_js() === 1;
  },
  async won(): Promise<boolean> {
    const m = await loadWasm();
    return m._g2048_won_js() === 1;
  },
  async canMove(): Promise<boolean> {
    const m = await loadWasm();
    return m._g2048_can_move_js() === 1;
  },
};
```

- [ ] **Step 1.4.4: 添加加载脚本到 index.html**

修改 `index.html`：在 `<script type="module" ...>` 之前加 `<script src="/wasm/games.js"></script>`

```html
<script src="/wasm/games.js"></script>
<script type="module" src="/src/main.tsx"></script>
```

- [ ] **Step 1.4.5: 验证加载**

修改 `src/App.tsx` 临时打印 `window.GameModule`：

```tsx
useEffect(() => { console.log('GameModule:', !!window.GameModule); }, []);
```

Run: `npm run dev`，打开浏览器控制台
Expected: 看到 "GameModule: true"

- [ ] **Step 1.4.6: 提交**

```bash
git add engineering/apps/games/web/src/wasm engineering/apps/games/web/src/App.tsx engineering/apps/games/web/index.html
git commit -m "feat(games-web): add wasm loader and typed bindings"
```

---

## Task 1.5: 2048 游戏 Hooks + Canvas 渲染器

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\src\games\g2048\types.ts`
- Create: `D:\code\book\engineering\apps\games\web\src\games\g2048\renderer.ts`
- Create: `D:\code\book\engineering\apps\games\web\src\games\g2048\useG2048.ts`
- Create: `D:\code\book\engineering\apps\games\web\tests\unit\wasm\binding.test.ts`

**Interfaces:**
- Consumes: `game2048` bindings（Task 1.4）
- Produces: `useG2048()` Hook + `<Board />` 渲染组件

- [ ] **Step 1.5.1: 编写测试 - bindings 应可加载**

```ts
// tests/unit/wasm/binding.test.ts
import { describe, it, expect, beforeAll } from 'vitest';
import { loadWasm } from '@/wasm/loader';

describe('WASM bindings', () => {
  beforeAll(async () => {
    // 在 Node 环境模拟 window
    (global as any).window = (global as any).window || {};
    (window as any).GameModule = async () => mockExports;
  });

  it('loadWasm resolves with exports', async () => {
    const m = await loadWasm();
    expect(m).toBeDefined();
  });
});

const mockExports = {
  _g2048_init_js: () => {},
  _g2048_move_js: () => 1,
  _g2048_tile_js: () => 0,
  _g2048_score_js: () => 0,
  _g2048_game_over_js: () => 0,
  _g2048_won_js: () => 0,
  _g2048_can_move_js: () => 1,
};
```

- [ ] **Step 1.5.2: 创建 types.ts**

```ts
// src/games/g2048/types.ts
export type Direction = 0 | 1 | 2 | 3;
export type Difficulty = 4 | 5 | 6; // 棋盘大小

export interface Board {
  tiles: number[][]; // 4x4, 5x5, 6x6
  score: number;
  gameOver: boolean;
  won: boolean;
}
```

- [ ] **Step 1.5.3: 实现 useG2048 Hook**

```ts
// src/games/g2048/useG2048.ts
import { useState, useEffect, useCallback } from 'react';
import { game2048 } from '@/wasm/bindings';
import type { Board, Direction } from './types';

export function useG2048() {
  const [board, setBoard] = useState<Board | null>(null);
  const [error, setError] = useState<string | null>(null);

  const newGame = useCallback(async (seed?: number) => {
    try {
      await g2048.init(seed ?? Date.now());
      await refresh();
    } catch (e) {
      setError((e as Error).message);
    }
  }, []);

  const refresh = useCallback(async () => {
    const size = 4;
    const tiles: number[][] = [];
    for (let r = 0; r < size; r++) {
      tiles[r] = [];
      for (let c = 0; c < size; c++) {
        tiles[r][c] = await g2048.tile(r, c);
      }
    }
    setBoard({
      tiles,
      score: await g2048.score(),
      gameOver: await g2048.gameOver(),
      won: await g2048.won(),
    });
  }, []);

  const move = useCallback(async (dir: Direction) => {
    const moved = await g2048.move(dir);
    if (moved) await refresh();
    return moved;
  }, [refresh]);

  useEffect(() => { newGame(); }, [newGame]);

  return { board, error, newGame, move };
}
```

- [ ] **Step 1.5.4: 实现 Canvas 渲染器**

```ts
// src/games/g2048/renderer.ts
export function renderBoard(
  ctx: CanvasRenderingContext2D,
  tiles: number[][],
  cellSize = 100,
  padding = 10
) {
  const size = tiles.length;
  // 背景
  ctx.fillStyle = '#bbada0';
  ctx.fillRect(0, 0, size * cellSize, size * cellSize);

  for (let r = 0; r < size; r++) {
    for (let c = 0; c < size; c++) {
      const v = tiles[r][c];
      const x = c * cellSize + padding;
      const y = r * cellSize + padding;
      const sz = cellSize - padding * 2;
      ctx.fillStyle = v === 0 ? '#cdc1b4' : tileColor(v);
      roundRect(ctx, x, y, sz, sz, 6);
      ctx.fill();

      if (v !== 0) {
        ctx.fillStyle = v <= 4 ? '#776e65' : '#f9f6f2';
        ctx.font = `bold ${sz * 0.45}px Arial`;
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(String(v), x + sz / 2, y + sz / 2);
      }
    }
  }
}

function tileColor(v: number): string {
  const map: Record<number, string> = {
    2: '#eee4da', 4: '#ede0c8', 8: '#f2b179', 16: '#f59563',
    32: '#f67c5f', 64: '#f65e3b', 128: '#edcf72', 256: '#edcc61',
    512: '#edc850', 1024: '#edc53f', 2048: '#edc22e',
  };
  return map[v] || '#3c3a32';
}

function roundRect(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number) {
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.lineTo(x + w - r, y);
  ctx.quadraticCurveTo(x + w, y, x + w, y + r);
  ctx.lineTo(x + w, y + h - r);
  ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
  ctx.lineTo(x + r, y + h);
  ctx.quadraticCurveTo(x, y + h, x, y + h - r);
  ctx.lineTo(x, y + r);
  ctx.quadraticCurveTo(x, y, x + r, y);
  ctx.closePath();
}
```

- [ ] **Step 1.5.5: 运行测试**

Run: `cd "D:\code\book\engineering\apps\games\web" && npm test`
Expected: 测试通过

- [ ] **Step 1.5.6: 提交**

```bash
git add engineering/apps/games/web/src/games/g2048 engineering/apps/games/web/tests
git commit -m "feat(games-web): 2048 game hook and canvas renderer"
```

---

## Task 1.6: 2048 游戏页面

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\src\pages\Game2048\index.tsx`
- Create: `D:\code\book\engineering\apps\games\web\src\router.tsx`
- Create: `D:\code\book\engineering\apps\games\web\src\pages\Home\index.tsx`

**Interfaces:**
- Consumes: `useG2048()` + `renderBoard()`（Task 1.5）
- Produces: `/2048` 路由可访问并能玩

- [ ] **Step 1.6.1: 创建 Home 页面（游戏中心）**

```tsx
// src/pages/Home/index.tsx
import { Link } from 'react-router-dom';

export function Home() {
  return (
    <div className="min-h-screen bg-gray-50 dark:bg-gray-900 p-8">
      <h1 className="text-4xl font-bold text-center mb-8">🎮 游戏中心</h1>
      <div className="grid grid-cols-1 md:grid-cols-3 gap-6 max-w-4xl mx-auto">
        <GameCard title="2048" desc="滑动合并" path="/2048" />
        <GameCard title="贪吃蛇" desc="控制蛇身" path="/snake" disabled />
        <GameCard title="数独" desc="逻辑推理" path="/sudoku" disabled />
      </div>
    </div>
  );
}

function GameCard({ title, desc, path, disabled }: {
  title: string; desc: string; path: string; disabled?: boolean;
}) {
  if (disabled) {
    return (
      <div className="p-6 bg-gray-100 dark:bg-gray-800 rounded-lg opacity-50">
        <h2 className="text-2xl font-bold">{title}</h2>
        <p className="text-gray-500">{desc}</p>
        <p className="text-sm text-gray-400 mt-2">敬请期待</p>
      </div>
    );
  }
  return (
    <Link to={path} className="p-6 bg-white dark:bg-gray-800 rounded-lg shadow hover:shadow-lg transition-shadow">
      <h2 className="text-2xl font-bold">{title}</h2>
      <p className="text-gray-500">{desc}</p>
    </Link>
  );
}
```

- [ ] **Step 1.6.2: 创建 2048 游戏页**

```tsx
// src/pages/Game2048/index.tsx
import { useEffect, useRef } from 'react';
import { useG2048 } from '@/games/g2048/useG2048';
import { renderBoard } from '@/games/g2048/renderer';
import { Button } from '@shared/ui/Button';

export function Game2048() {
  const { board, error, newGame, move } = useG2048();
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    if (!board || !canvasRef.current) return;
    const ctx = canvasRef.current.getContext('2d')!;
    renderBoard(ctx, board.tiles);
  }, [board]);

  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      const map: Record<string, 0 | 1 | 2 | 3> = {
        ArrowUp: 0, ArrowDown: 1, ArrowLeft: 2, ArrowRight: 3,
        w: 0, s: 1, a: 2, d: 3, W: 0, S: 1, A: 2, D: 3,
      };
      const dir = map[e.key];
      if (dir !== undefined) { e.preventDefault(); move(dir); }
      if (e.key === 'r' || e.key === 'R') newGame();
    };
    document.addEventListener('keydown', handler);
    return () => document.removeEventListener('keydown', handler);
  }, [move, newGame]);

  if (error) {
    return (
      <div className="p-8 text-center">
        <p className="text-red-500">WASM 加载失败：{error}</p>
        <p className="text-sm text-gray-500 mt-2">运行 npm run wasm:build 重新编译</p>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-g2048-bg dark:bg-gray-900 p-8">
      <h1 className="text-4xl font-bold text-center mb-4 text-gray-700 dark:text-white">2048</h1>
      <div className="text-center mb-4">
        分数：<span className="font-bold">{board?.score ?? 0}</span>
        {board?.gameOver && <span className="ml-4 text-red-500">游戏结束</span>}
        {board?.won && <span className="ml-4 text-yellow-500">🎉 达成 2048！</span>}
      </div>
      <canvas ref={canvasRef} width={400} height={400} className="mx-auto rounded-lg" />
      <div className="text-center mt-4">
        <Button onClick={() => newGame()}>新游戏 (R)</Button>
      </div>
      <p className="text-center text-sm text-gray-500 mt-4">
        WASD 或方向键移动 · R 重新开始
      </p>
    </div>
  );
}
```

- [ ] **Step 1.6.3: 创建路由**

```tsx
// src/router.tsx
import { createBrowserRouter, RouterProvider } from 'react-router-dom';
import { Home } from '@/pages/Home';
import { Game2048 } from '@/pages/Game2048';

const router = createBrowserRouter([
  { path: '/', element: <Home /> },
  { path: '/2048', element: <Game2048 /> },
]);

export function Router() {
  return <RouterProvider router={router} />;
}
```

- [ ] **Step 1.6.4: 更新 App.tsx 使用路由**

```tsx
// src/App.tsx
import { Router } from './router';

export function App() {
  return <Router />;
}
```

- [ ] **Step 1.6.5: 启动并验证可玩**

Run: `cd "D:\code\book\engineering\apps\games\web" && npm run dev`
Expected: 浏览器打开 http://localhost:5173/2048 能玩 2048

- [ ] **Step 1.6.6: 提交**

```bash
git add engineering/apps/games/web/src
git commit -m "feat(games-web): 2048 playable page with keyboard input"
```

---

# MVP-1 完成验证

- [ ] 运行 `npm run dev` 启动开发服务器
- [ ] 访问 `/` 看到游戏中心首页
- [ ] 访问 `/2048` 能玩 2048（移动、合并、计分、新游戏）
- [ ] 暗色主题切换工作
- [ ] 编译无 TS 错误

🎉 **MVP-1 完成**。可以交付：可玩的现代化 2048 + 统一设计系统。

---

# MVP-2: 贪吃蛇 + 数独 (W3-W4)

> **Demo 能力**：三个游戏全部可玩（贪吃蛇、数独新增 Web 版）。

## Task 2.1: C → WASM 添加 snake 模块

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\wasm-src\snake.h`
- Create: `D:\code\book\engineering\apps\games\web\wasm-src\snake.c`
- Modify: `D:\code\book\engineering\apps\games\web\wasm-src\binding.c`
- Modify: `D:\code\book\engineering\apps\games\web\wasm-src\build.sh`

**Interfaces:**
- Consumes: 现有 `engineering\apps\games\core\snake_core.{c,h}`
- Produces: WASM 暴露 `snake_*` 函数

- [ ] **Step 2.1.1: 创建 snake.h**

```c
// wasm-src/snake.h
#ifndef SNAKE_H
#define SNAKE_H

#include <stdbool.h>
#define SNAKE_W 20
#define SNAKE_H 20
#define SNAKE_MAX 200

typedef enum { SNAKE_UP, SNAKE_DOWN, SNAKE_LEFT, SNAKE_RIGHT } SnakeDir;

typedef struct { int x, y; } Pt;

typedef struct {
  Pt body[SNAKE_MAX];
  int len;
  Pt food;
  int score;
  bool over;
  SnakeDir dir, next;
} SnakeGame;

void snake_init(SnakeGame *g, int seed, int difficulty);
void snake_tick(SnakeGame *g);
void snake_input(SnakeGame *g, SnakeDir d);

#endif
```

- [ ] **Step 2.1.2: 创建 snake.c**

从 `engineering\apps\games\core\snake_core.c` 复制并简化实现（保留核心移动/吃食物/碰撞检测）。

- [ ] **Step 2.1.3: 扩展 binding.c**

在 `binding.c` 末尾添加：

```c
#include "snake.h"
static SnakeGame s_state;

EMSCRIPTEN_KEEPALIVE void snake_init_js(int seed, int diff) { snake_init(&s_state, seed, diff); }
EMSCRIPTEN_KEEPALIVE void snake_tick_js(void) { snake_tick(&s_state); }
EMSCRIPTEN_KEEPALIVE void snake_input_js(int d) { snake_input(&s_state, (SnakeDir)d); }
EMSCRIPTEN_KEEPALIVE int snake_len_js(void) { return s_state.len; }
EMSCRIPTEN_KEEPALIVE int snake_body_x_js(int i) { return s_state.body[i].x; }
EMSCRIPTEN_KEEPALIVE int snake_body_y_js(int i) { return s_state.body[i].y; }
EMSCRIPTEN_KEEPALIVE int snake_food_x_js(void) { return s_state.food.x; }
EMSCRIPTEN_KEEPALIVE int snake_food_y_js(void) { return s_state.food.y; }
EMSCRIPTEN_KEEPALIVE int snake_score_js(void) { return s_state.score; }
EMSCRIPTEN_KEEPALIVE int snake_over_js(void) { return s_state.over ? 1 : 0; }
```

- [ ] **Step 2.1.4: 更新 build.sh**

把 `$WASM_SRC/snake.c` 加入编译命令，并把所有 `_snake_*_js` 加入 `EXPORTED_FUNCTIONS`。

- [ ] **Step 2.1.5: 编译验证**

Run: `bash wasm-src/build.sh`
Expected: 编译成功

- [ ] **Step 2.1.6: 提交**

```bash
git add engineering/apps/games/web/wasm-src
git commit -m "feat(games-web): add snake module to wasm"
```

---

## Task 2.2: 贪吃蛇游戏 Hook + Canvas

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\src\games\snake\useSnake.ts`
- Create: `D:\code\book\engineering\apps\games\web\src\games\snake\renderer.ts`

**Interfaces:**
- Consumes: `_snake_*_js` WASM 函数
- Produces: `useSnake()` Hook + 渲染函数

- [ ] **Step 2.2.1: 添加 snake 类型到 wasm/types.ts**

```ts
export interface SnakeExports {
  _snake_init_js(seed: number, difficulty: number): void;
  _snake_tick_js(): void;
  _snake_input_js(dir: number): void;
  _snake_len_js(): number;
  _snake_body_x_js(i: number): number;
  _snake_body_y_js(i: number): number;
  _snake_food_x_js(): number;
  _snake_food_y_js(): number;
  _snake_score_js(): number;
  _snake_over_js(): number;
}
```

- [ ] **Step 2.2.2: 扩展 bindings.ts 添加 snake**

```ts
export const snake = {
  async init(seed: number, difficulty = 0) { (await loadWasm())._snake_init_js(seed, difficulty); },
  async tick() { (await loadWasm())._snake_tick_js(); },
  async input(dir: 0|1|2|3) { (await loadWasm())._snake_input_js(dir); },
  async len() { return (await loadWasm())._snake_len_js(); },
  async bodyX(i: number) { return (await loadWasm())._snake_body_x_js(i); },
  async bodyY(i: number) { return (await loadWasm())._snake_body_y_js(i); },
  async foodX() { return (await loadWasm())._snake_food_x_js(); },
  async foodY() { return (await loadWasm())._snake_food_y_js(); },
  async score() { return (await loadWasm())._snake_score_js(); },
  async over() { return (await loadWasm())._snake_over_js() === 1; },
};
```

- [ ] **Step 2.2.3: 实现 useSnake Hook**

```ts
// src/games/snake/useSnake.ts
import { useEffect, useRef, useState, useCallback } from 'react';
import { snake } from '@/wasm/bindings';

export function useSnake(difficulty = 0) {
  const [state, setState] = useState({ score: 0, over: false });
  const rafRef = useRef<number>();

  const tick = useCallback(async () => {
    await snake.tick();
    setState({
      score: await snake.score(),
      over: await snake.over(),
    });
  }, []);

  useEffect(() => {
    let last = performance.now();
    const interval = [180, 120, 80][difficulty] ?? 180;
    const loop = async (now: number) => {
      if (now - last >= interval) {
        await tick();
        last = now;
      }
      rafRef.current = requestAnimationFrame(loop);
    };
    snake.init(Date.now(), difficulty).then(() => rafRef.current = requestAnimationFrame(loop));
    return () => { if (rafRef.current) cancelAnimationFrame(rafRef.current); };
  }, [difficulty, tick]);

  return state;
}
```

- [ ] **Step 2.2.4: 提交**

```bash
git add engineering/apps/games/web/src/games/snake engineering/apps/games/web/src/wasm
git commit -m "feat(games-web): snake game hook and bindings"
```

---

## Task 2.3: 贪吃蛇游戏页面

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\src\pages\Snake\index.tsx`
- Modify: `D:\code\book\engineering\apps\games\web\src\router.tsx`
- Modify: `D:\code\book\engineering\apps\games\web\src\pages\Home\index.tsx`

- [ ] **Step 2.3.1: 创建 Snake 页面**

```tsx
// src/pages/Snake/index.tsx
import { useEffect, useRef } from 'react';
import { useSnake } from '@/games/snake/useSnake';
import { snake } from '@/wasm/bindings';

export function Snake() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const state = useSnake(0);

  useEffect(() => {
    if (!canvasRef.current) return;
    const ctx = canvasRef.current.getContext('2d')!;
    const gs = 20;
    ctx.fillStyle = '#fff';
    ctx.fillRect(0, 0, 400, 400);
    // 蛇
    (async () => {
      const len = await snake.len();
      ctx.fillStyle = '#2ecc71';
      for (let i = 0; i < len; i++) {
        const x = await snake.bodyX(i), y = await snake.bodyY(i);
        ctx.fillRect(x * gs + 1, y * gs + 1, gs - 2, gs - 2);
      }
      // 食物
      ctx.fillStyle = '#e74c3c';
      const fx = await snake.foodX(), fy = await snake.foodY();
      ctx.fillRect(fx * gs + 1, fy * gs + 1, gs - 2, gs - 2);
    })();
  }, [state.score, state.over]);

  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      const m: Record<string, 0|1|2|3> = { ArrowUp:0, ArrowDown:1, ArrowLeft:2, ArrowRight:3, w:0,s:1,a:2,d:3 };
      const d = m[e.key];
      if (d !== undefined) { e.preventDefault(); snake.input(d); }
    };
    document.addEventListener('keydown', handler);
    return () => document.removeEventListener('keydown', handler);
  }, []);

  return (
    <div className="min-h-screen bg-snake-board dark:bg-gray-900 p-8 text-center">
      <h1 className="text-4xl font-bold mb-4">🐍 贪吃蛇</h1>
      <div className="text-lg mb-4">分数：<span className="font-bold">{state.score}</span></div>
      <canvas ref={canvasRef} width={400} height={400} className="mx-auto border-2 border-gray-300" />
      {state.over && <p className="mt-4 text-red-500 text-xl">游戏结束！按 R 重新开始</p>}
      <p className="mt-4 text-sm text-gray-500">WASD 或方向键移动</p>
    </div>
  );
}
```

- [ ] **Step 2.3.2: 注册路由**

在 `router.tsx` 加 `{ path: '/snake', element: <Snake /> }`，并在 Home 页开启蛇的链接。

- [ ] **Step 2.3.3: 验证贪吃蛇可玩**

Run: `npm run dev`，访问 `/snake`
Expected: 蛇能移动、吃食物增长、撞墙结束

- [ ] **Step 2.3.4: 提交**

```bash
git add engineering/apps/games/web/src/pages/Snake engineering/apps/games/web/src/router.tsx engineering/apps/games/web/src/pages/Home
git commit -m "feat(games-web): snake playable page"
```

---

## Task 2.4: 数独 C 模块（从 C++ 移植）

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\wasm-src\sudoku.h`
- Create: `D:\code\book\engineering\apps\games\web\wasm-src\sudoku.c`
- Modify: `D:\code\book\engineering\apps\games\web\wasm-src\binding.c`
- Modify: `D:\code\book\engineering\apps\games\web\wasm-src\build.sh`

**Interfaces:**
- Consumes: 现有 `engineering\apps\games\sudoku\sudoku.{cpp,h}`
- Produces: WASM 暴露 `sudoku_*` 函数

- [ ] **Step 2.4.1: 创建 sudoku.h（纯 C 版）**

```c
// wasm-src/sudoku.h
#ifndef SUDOKU_H
#define SUDOKU_H

#include <stdbool.h>
#define SUDOKU_SIZE 9

typedef struct { int value; bool given; bool conflict; } SudokuCell;
typedef struct {
  SudokuCell board[SUDOKU_SIZE][SUDOKU_SIZE];
  int solution[SUDOKU_SIZE][SUDOKU_SIZE];
  int difficulty; // 0=简, 1=中, 2=难
  bool over;
} SudokuGame;

void sudoku_init(SudokuGame *g, int difficulty, int seed);
int  sudoku_set(SudokuGame *g, int row, int col, int num);
void sudoku_erase(SudokuGame *g, int row, int col);
int  sudoku_hint(const SudokuGame *g, int row, int col);
bool sudoku_is_valid(const SudokuGame *g, int row, int col, int num);

#endif
```

- [ ] **Step 2.4.2: 创建 sudoku.c**

从 `engineering\apps\games\sudoku\sudoku.cpp` 移植核心算法（`generate_full_board` + 挖洞 + 求解），重写为纯 C。关键函数签名匹配新 header。

- [ ] **Step 2.4.3: 扩展 binding.c 添加 sudoku**

```c
#include "sudoku.h"
static SudokuGame sd_state;

EMSCRIPTEN_KEEPALIVE void sudoku_init_js(int d, int seed) { sudoku_init(&sd_state, d, seed); }
EMSCRIPTEN_KEEPALIVE int sudoku_set_js(int r, int c, int n) { return sudoku_set(&sd_state, r, c, n); }
EMSCRIPTEN_KEEPALIVE void sudoku_erase_js(int r, int c) { sudoku_erase(&sd_state, r, c); }
EMSCRIPTEN_KEEPALIVE int sudoku_value_js(int r, int c) { return sd_state.board[r][c].value; }
EMSCRIPTEN_KEEPALIVE int sudoku_given_js(int r, int c) { return sd_state.board[r][c].given ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE int sudoku_conflict_js(int r, int c) { return sd_state.board[r][c].conflict ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE int sudoku_over_js(void) { return sd_state.over ? 1 : 0; }
```

- [ ] **Step 2.4.4: 编译验证**

Run: `bash wasm-src/build.sh`
Expected: 编译成功

- [ ] **Step 2.4.5: 提交**

```bash
git add engineering/apps/games/web/wasm-src
git commit -m "feat(games-web): add sudoku to wasm (port from C++)"
```

---

## Task 2.5: 数独游戏 Hook + UI

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\src\games\sudoku\types.ts`
- Create: `D:\code\book\engineering\apps\games\web\src\games\sudoku\useSudoku.ts`

- [ ] **Step 2.5.1: 创建 types.ts**

```ts
// src/games/sudoku/types.ts
export interface Cell { value: number; given: boolean; conflict: boolean; }
export interface Board { cells: Cell[][]; over: boolean; difficulty: number; }
```

- [ ] **Step 2.5.2: 添加 WASM 类型**

在 `wasm/types.ts` 加 `SudokuExports`。

- [ ] **Step 2.5.3: 添加 bindings**

```ts
export const sudoku = {
  async init(d: 0|1|2, seed: number) { (await loadWasm())._sudoku_init_js(d, seed); },
  async set(r: number, c: number, n: number) { return (await loadWasm())._sudoku_set_js(r, c, n); },
  async erase(r: number, c: number) { (await loadWasm())._sudoku_erase_js(r, c); },
  async value(r: number, c: number) { return (await loadWasm())._sudoku_value_js(r, c); },
  async given(r: number, c: number) { return (await loadWasm())._sudoku_given_js(r, c) === 1; },
  async conflict(r: number, c: number) { return (await loadWasm())._sudoku_conflict_js(r, c) === 1; },
  async over() { return (await loadWasm())._sudoku_over_js() === 1; },
};
```

- [ ] **Step 2.5.4: 实现 useSudoku Hook**

```ts
// src/games/sudoku/useSudoku.ts
import { useEffect, useState, useCallback } from 'react';
import { sudoku } from '@/wasm/bindings';
import type { Board, Cell } from './types';

export function useSudoku(difficulty: 0|1|2 = 0) {
  const [board, setBoard] = useState<Board | null>(null);

  const refresh = useCallback(async () => {
    const cells: Cell[][] = [];
    for (let r = 0; r < 9; r++) {
      cells[r] = [];
      for (let c = 0; c < 9; c++) {
        cells[r][c] = {
          value: await sudoku.value(r, c),
          given: await sudoku.given(r, c),
          conflict: await sudoku.conflict(r, c),
        };
      }
    }
    setBoard({ cells, over: await sudoku.over(), difficulty });
  }, [difficulty]);

  const newGame = useCallback(async () => {
    await sudoku.init(difficulty, Date.now());
    await refresh();
  }, [difficulty, refresh]);

  const setCell = useCallback(async (r: number, c: number, n: number) => {
    await sudoku.set(r, c, n);
    await refresh();
  }, [refresh]);

  const eraseCell = useCallback(async (r: number, c: number) => {
    await sudoku.erase(r, c);
    await refresh();
  }, [refresh]);

  useEffect(() => { newGame(); }, [newGame]);

  return { board, newGame, setCell, eraseCell };
}
```

---

## Task 2.6: 数独游戏页面

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\src\pages\Sudoku\index.tsx`
- Modify: `D:\code\book\engineering\apps\games\web\src\router.tsx`
- Modify: `D:\code\book\engineering\apps\games\web\src\pages\Home\index.tsx`

- [ ] **Step 2.6.1: 创建 Sudoku 页面**

```tsx
// src/pages/Sudoku/index.tsx
import { useState } from 'react';
import { useSudoku } from '@/games/sudoku/useSudoku';
import { Button } from '@shared/ui/Button';

export function Sudoku() {
  const [difficulty, setDifficulty] = useState<0|1|2>(0);
  const { board, setCell, eraseCell, newGame } = useSudoku(difficulty);

  return (
    <div className="min-h-screen bg-gray-50 dark:bg-gray-900 p-8 text-center">
      <h1 className="text-4xl font-bold mb-4">🔢 数独</h1>
      <div className="mb-4 flex justify-center gap-2">
        {(['简单','中等','困难'] as const).map((label, i) => (
          <Button key={i} variant={difficulty === i ? 'primary' : 'ghost'} onClick={() => setDifficulty(i as 0|1|2)}>
            {label}
          </Button>
        ))}
      </div>
      {board?.over && <p className="text-green-500 text-xl mb-4">🎉 完成！</p>}
      <div className="inline-block border-4 border-sudoku-grid">
        {board?.cells.map((row, r) => (
          <div key={r} className="flex">
            {row.map((cell, c) => (
              <button
                key={c}
                className={`w-12 h-12 border border-gray-300 text-lg font-bold
                  ${(c === 2 || c === 5) ? 'border-r-4' : ''} ${(r === 2 || r === 5) ? 'border-b-4' : ''}
                  ${cell.given ? 'text-gray-900 bg-gray-100' : 'text-primary-500'}
                  ${cell.conflict ? 'text-red-500 bg-red-50' : ''}`}
                onClick={() => {
                  const v = prompt('输入 1-9（0 删除）');
                  const n = parseInt(v ?? '0');
                  if (n === 0) eraseCell(r, c);
                  else setCell(r, c, n);
                }}
              >
                {cell.value !== 0 ? cell.value : ''}
              </button>
            ))}
          </div>
        ))}
      </div>
      <div className="mt-4"><Button onClick={newGame}>新游戏</Button></div>
    </div>
  );
}
```

- [ ] **Step 2.6.2: 注册路由 + Home 启用链接**

在 `router.tsx` 加 `{ path: '/sudoku', element: <Sudoku /> }`，Home 页去掉 sudoku 的 `disabled`。

- [ ] **Step 2.6.3: 验证数独可玩**

Run: `npm run dev`，访问 `/sudoku`
Expected: 数独能选题、填数、显示冲突

- [ ] **Step 2.6.4: 提交**

```bash
git add engineering/apps/games/web/src/games/sudoku engineering/apps/games/web/src/pages/Sudoku engineering/apps/games/web/src/router.tsx engineering/apps/games/web/src/pages/Home
git commit -m "feat(games-web): sudoku playable page"
```

---

# MVP-2 完成验证

- [ ] 2048、贪吃蛇、数独三个游戏都可玩
- [ ] 每个游戏都能新开、计分、游戏结束检测

🎉 **MVP-2 完成**。三个游戏 Web 版全部就绪。

---

# MVP-3: 游戏 web 收尾 (W5-W7)

> **Demo 能力**：游戏 web 完整功能（动画、撤销、笔记、提示、成就、暗色、响应式）。

## Task 3.1: 动画系统（2048 方块滑动 + 弹出）

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\src\games\g2048\animation.ts`

- [ ] **Step 3.1.1: 实现缓动函数**

```ts
// src/games/g2048/animation.ts
export const easing = {
  slideIn: 'cubic-bezier(0.25, 0.1, 0.25, 1)',
  popIn: 'cubic-bezier(0.18, 0.89, 0.32, 1.28)',
};
```

- [ ] **Step 3.1.2: 添加 CSS 动画**

在 `index.css` 末尾添加：

```css
@layer components {
  .tile-slide {
    transition: transform 200ms cubic-bezier(0.25, 0.1, 0.25, 1);
  }
  .tile-pop {
    animation: tile-pop 200ms cubic-bezier(0.18, 0.89, 0.32, 1.28);
  }
  @keyframes tile-pop {
    0% { transform: scale(0); }
    50% { transform: scale(1.2); }
    100% { transform: scale(1); }
  }
}
```

- [ ] **Step 3.1.3: 在 2048 页面应用动画类**

修改 `pages/Game2048/index.tsx`：在 Canvas 旁加一个过渡层（或者切换到 DOM 渲染方块，便于 CSS 动画）。本次先保留 Canvas + 添加新方块动画 demo。

- [ ] **Step 3.1.4: 验证动画效果**

Run: `npm run dev`，访问 `/2048`，看新方块出现时有弹出动画

- [ ] **Step 3.1.5: 提交**

```bash
git add engineering/apps/games/web/src/games/g2048/animation.ts engineering/apps/games/web/src/index.css
git commit -m "feat(games-web): 2048 tile animations"
```

---

## Task 3.2: 撤销/重做（2048）

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\src\games\g2048\history.ts`
- Modify: `D:\code\book\engineering\apps\games\web\src\games\g2048\useG2048.ts`
- Modify: `D:\code\book\engineering\apps\games\web\src\pages\Game2048\index.tsx`

- [ ] **Step 3.2.1: 实现历史栈**

```ts
// src/games/g2048/history.ts
export interface Snapshot { tiles: number[][]; score: number; }

export class History {
  private stack: Snapshot[] = [];
  constructor(private max = 20) {}

  push(s: Snapshot) {
    this.stack.push(s);
    if (this.stack.length > this.max) this.stack.shift();
  }
  pop(): Snapshot | undefined { return this.stack.pop(); }
  get size() { return this.stack.length; }
}
```

- [ ] **Step 3.2.2: 在 useG2048 集成**

修改 `useG2048.ts`：在每次 `move` 前 push 当前状态；添加 `undo()` 方法。

- [ ] **Step 3.2.3: UI 添加撤销按钮**

在 2048 页面加 `<Button onClick={undo}>撤销</Button>`，绑定 `U` 键。

- [ ] **Step 3.2.4: 验证**

Run: 玩几局，按 U 撤销看是否回到上一步

- [ ] **Step 3.2.5: 提交**

```bash
git add engineering/apps/games/web/src/games/g2048
git commit -m "feat(games-web): 2048 undo/redo"
```

---

## Task 3.3: 数独笔记 + 提示

**Files:**
- Modify: `D:\code\book\engineering\apps\games\web\wasm-src\sudoku.h`
- Modify: `D:\code\book\engineering\apps\games\web\wasm-src\sudoku.c`
- Modify: `D:\code\book\engineering\apps\games\web\src\games\sudoku\useSudoku.ts`
- Modify: `D:\code\book\engineering\apps\games\web\src\pages\Sudoku\index.tsx`

- [ ] **Step 3.3.1: 扩展 sudoku 数据结构**

在 `SudokuCell` 加 `int notes`（位掩码，每位表示一个候选数）。

- [ ] **Step 3.3.2: 添加 note 操作函数**

```c
void sudoku_toggle_note(SudokuGame *g, int r, int c, int num);
int  sudoku_notes_at(const SudokuGame *g, int r, int c);
```

- [ ] **Step 3.3.3: 更新 bindings 和 UI**

在 Sudoku 页面加笔记按钮 + 提示按钮。

- [ ] **Step 3.3.4: 提交**

```bash
git add engineering/apps/games/web/wasm-src engineering/apps/games/web/src/games/sudoku engineering/apps/games/web/src/pages/Sudoku
git commit -m "feat(games-web): sudoku notes and hints"
```

---

## Task 3.4: 触摸输入（响应式）

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\src\games\g2048\gesture.ts`
- Modify: `D:\code\book\engineering\apps\games\web\src\pages\Game2048\index.tsx`
- Modify: `D:\code\book\engineering\apps\games\web\src\pages\Snake\index.tsx`

- [ ] **Step 3.4.1: 实现滑动手势检测**

```ts
// src/games/g2048/gesture.ts
export function detectSwipe(start: {x:number,y:number}, end: {x:number,y:number}): 0|1|2|3|null {
  const dx = end.x - start.x, dy = end.y - start.y;
  if (Math.abs(dx) < 30 && Math.abs(dy) < 30) return null;
  if (Math.abs(dx) > Math.abs(dy)) return dx > 0 ? 3 : 2;
  return dy > 0 ? 1 : 0;
}
```

- [ ] **Step 3.4.2: 在 2048 页面绑定 touchstart/touchend**

- [ ] **Step 3.4.3: 在 snake 页面加屏幕方向按钮（移动端）**

- [ ] **Step 3.4.4: 测试移动端**

用浏览器 DevTools 切到移动设备视图，验证手势操作

- [ ] **Step 3.4.5: 提交**

```bash
git add engineering/apps/games/web/src/games/g2048/gesture.ts engineering/apps/games/web/src/pages/Game2048 engineering/apps/games/web/src/pages/Snake
git commit -m "feat(games-web): touch/swipe input for mobile"
```

---

## Task 3.5: 成就系统 + localStorage 集成

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\shared\web\src\storage\safeStorage.ts`
- Create: `D:\code\book\engineering\apps\games\web\src\stores\achievements.ts`
- Create: `D:\code\book\engineering\apps\games\web\src\components\AchievementToast.tsx`

- [ ] **Step 3.5.1: 实现 storage 封装**

```ts
// shared/web/src/storage/safeStorage.ts
export function safeGet<T>(key: string, fallback: T): T {
  try {
    const v = localStorage.getItem(key);
    return v ? JSON.parse(v) : fallback;
  } catch { return fallback; }
}
export function safeSet<T>(key: string, value: T): void {
  try { localStorage.setItem(key, JSON.stringify(value)); } catch {}
}
```

- [ ] **Step 3.5.2: 实现成就 store**

```ts
// src/stores/achievements.ts
import { create } from 'zustand';
import { safeGet, safeSet } from '@shared/storage/safeStorage';

export interface Achievement {
  id: string; name: string; desc: string;
}

const ALL: Achievement[] = [
  { id: 'first-2048', name: '初识 2048', desc: '第一次玩 2048' },
  { id: 'merge-128', name: '合并达人', desc: '合并到 128' },
  { id: 'snake-50', name: '蛇中豪杰', desc: '贪吃蛇吃到 50 分' },
  { id: 'sudoku-easy', name: '数独新手', desc: '完成简单数独' },
];

export const useAchievements = create<{
  unlocked: string[];
  unlock: (id: string) => void;
}>(set => ({
  unlocked: safeGet('achievements', [] as string[]),
  unlock: (id) => set(s => {
    if (s.unlocked.includes(id)) return s;
    const next = [...s.unlocked, id];
    safeSet('achievements', next);
    return { unlocked: next };
  }),
}));
```

- [ ] **Step 3.5.3: 创建 toast 组件**

```tsx
// src/components/AchievementToast.tsx
import { AnimatePresence, motion } from 'framer-motion';

export function AchievementToast({ message }: { message: string | null }) {
  return (
    <AnimatePresence>
      {message && (
        <motion.div
          initial={{ opacity: 0, y: 50 }} animate={{ opacity: 1, y: 0 }} exit={{ opacity: 0, y: 50 }}
          className="fixed bottom-4 right-4 bg-primary-500 text-white px-4 py-2 rounded shadow-lg"
        >
          🏆 {message}
        </motion.div>
      )}
    </AnimatePresence>
  );
}
```

- [ ] **Step 3.5.4: 在游戏中触发成就**

修改 2048 页面：达成 2048 时调用 `unlock('merge-128')`。

- [ ] **Step 3.5.5: 提交**

```bash
git add engineering/apps/games/web/shared engineering/apps/games/web/src/stores engineering/apps/games/web/src/components
git commit -m "feat(games-web): achievements with localStorage persistence"
```

---

## Task 3.6: 国际化 + 主题切换

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\shared\web\src\i18n\i18n.ts`
- Create: `D:\code\book\engineering\apps\games\web\src\components\LanguageSwitcher.tsx`
- Create: `D:\code\book\engineering\apps\games\web\src\components\ThemeSwitcher.tsx`

- [ ] **Step 3.6.1: i18n 配置**

```ts
// shared/web/src/i18n/i18n.ts
import i18n from 'i18next';
import { initReactI18next } from 'react-i18next';

i18n.use(initReactI18next).init({
  resources: {
    zh: { translation: { play: '开始游戏', restart: '重新开始', score: '分数' } },
    en: { translation: { play: 'Play', restart: 'Restart', score: 'Score' } },
  },
  lng: localStorage.getItem('lang') ?? 'zh',
  fallbackLng: 'zh',
});
export default i18n;
```

- [ ] **Step 3.6.2: 在 main.tsx 引入**

```tsx
import '@shared/i18n/i18n';
```

- [ ] **Step 3.6.3: 切换器组件**

```tsx
// src/components/LanguageSwitcher.tsx
import { useTranslation } from 'react-i18next';
import { Button } from '@shared/ui/Button';

export function LanguageSwitcher() {
  const { i18n } = useTranslation();
  return (
    <Button variant="ghost" onClick={() => {
      const next = i18n.language === 'zh' ? 'en' : 'zh';
      i18n.changeLanguage(next);
      localStorage.setItem('lang', next);
    }}>
      {i18n.language === 'zh' ? 'EN' : '中'}
    </Button>
  );
}
```

- [ ] **Step 3.6.4: 提交**

```bash
git add engineering/apps/games/web/shared engineering/apps/games/web/src/components
git commit -m "feat(games-web): i18n and theme switchers"
```

---

## Task 3.7: E2E 测试 + 性能基准

**Files:**
- Create: `D:\code\book\engineering\apps\games\web\playwright.config.ts`
- Create: `D:\code\book\engineering\apps\games\web\tests\e2e\g2048.spec.ts`
- Create: `D:\code\book\engineering\apps\games\web\tests\e2e\snake.spec.ts`

- [ ] **Step 3.7.1: Playwright 配置**

```ts
// playwright.config.ts
import { defineConfig } from '@playwright/test';

export default defineConfig({
  testDir: './tests/e2e',
  use: { baseURL: 'http://localhost:5173' },
  webServer: { command: 'npm run dev', port: 5173, reuseExistingServer: true },
});
```

- [ ] **Step 3.7.2: 2048 E2E 测试**

```ts
// tests/e2e/g2048.spec.ts
import { test, expect } from '@playwright/test';

test('2048: 能玩并计分', async ({ page }) => {
  await page.goto('/2048');
  for (let i = 0; i < 30; i++) {
    await page.keyboard.press(['ArrowUp','ArrowRight','ArrowDown','ArrowLeft'][i % 4]);
    await page.waitForTimeout(60);
  }
  const score = await page.textContent('span:has-text("0")');
  expect(score).toBeDefined();
});
```

- [ ] **Step 3.7.3: 运行 E2E**

Run: `npx playwright install && npm run test:e2e`
Expected: 测试通过

- [ ] **Step 3.7.4: 提交**

```bash
git add engineering/apps/games/web/playwright.config.ts engineering/apps/games/web/tests
git commit -m "test(games-web): add e2e tests for 2048 and snake"
```

---

# MVP-3 完成验证

- [ ] 三个游戏有动画、撤销、笔记、提示
- [ ] 暗色主题切换正常
- [ ] 移动端手势可用
- [ ] 成就系统解锁、Toast 显示
- [ ] 中英文切换正常
- [ ] E2E 测试通过

🎉 **MVP-3 完成**。游戏 web 完整功能上线。

---

# MVP-4: 读书雷达基础设施 (W8-W9)

> **Demo 能力**：React + TS + Vite 项目搭好，旧数据能加载，design system 接入。

## Task 4.1: 项目脚手架（与 games-web 类似）

**Files:**
- Create: `D:\code\book\engineering\apps\web\reading-radar\package.json`
- Create: `D:\code\book\engineering\apps\web\reading-radar\vite.config.ts`
- Create: `D:\code\book\engineering\apps\web\reading-radar\tsconfig.json`
- Create: `D:\code\book\engineering\apps\web\reading-radar\tailwind.config.ts`

- [ ] **Step 4.1.1: package.json**（结构同 games-web，移除 WASM 相关）

```json
{
  "name": "reading-radar",
  "private": true,
  "version": "2.0.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "tsc -b && vite build",
    "preview": "vite preview",
    "server": "node server/dist/index.js",
    "server:dev": "tsx watch server/index.ts",
    "test": "vitest"
  },
  "dependencies": {
    "react": "^18.2.0",
    "react-dom": "^18.2.0",
    "react-router-dom": "^6.20.0",
    "zustand": "^4.5.0",
    "react-markdown": "^9.0.0",
    "remark-gfm": "^4.0.0",
    "rehype-mermaid": "^3.0.0",
    "express": "^5.0.0"
  },
  "devDependencies": {
    "@types/react": "^18.2.0",
    "@types/express": "^5.0.0",
    "@vitejs/plugin-react": "^4.2.0",
    "typescript": "^5.3.0",
    "vite": "^5.0.0",
    "tailwindcss": "^3.4.0",
    "tsx": "^4.7.0",
    "vitest": "^1.0.0"
  }
}
```

- [ ] **Step 4.1.2: vite.config.ts**（带 `/data` 别名指向现有 `data/`）

```ts
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
      '@data': path.resolve(__dirname, './data'),
      '@shared': path.resolve(__dirname, '../games/web/shared/web/src'),
    },
  },
});
```

- [ ] **Step 4.1.3: tsconfig.json + tailwind.config.ts**（与 games-web 同构）

- [ ] **Step 4.1.4: 安装依赖**

Run: `cd "D:\code\book\engineering\apps\web\reading-radar" && npm install`

- [ ] **Step 4.1.5: 提交**

```bash
git add engineering/apps/web/reading-radar/package.json engineering/apps/web/reading-radar/vite.config.ts engineering/apps/web/reading-radar/tsconfig.json engineering/apps/web/reading-radar/tailwind.config.ts
git commit -m "chore(reading-radar): scaffold vite + react + ts"
```

---

## Task 4.2: 数据层抽象

**Files:**
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\data\tech.ts`
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\data\questions.ts`
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\data\learn.ts`

- [ ] **Step 4.2.1: tech.ts**

```ts
// src/data/tech.ts
export interface TechItem {
  id: string; title: string;
  quadrant: 'language' | 'systems' | 'algorithms' | 'engineering';
  ring: 'basic' | 'intermediate' | 'advanced';
  desc: string;
}

// 运行时加载现有数据
export async function loadTechMeta(cat: 'c'|'cpp'|'ds'|'db'|'py'|'linux'|'vdb'): Promise<TechItem[]> {
  const mod = await import(`@data/quiz/tech.js`);
  return mod[`${cat.toUpperCase()}_TECH_DATA`] ?? [];
}
```

- [ ] **Step 4.2.2: questions.ts**

```ts
// src/data/questions.ts
export interface Question {
  id: string; type: string; difficulty: string;
  scenario: string; stem: string; code?: string;
  options: string[]; answer: string | string[] | boolean;
  explanation: string;
}

export async function loadQuestions(cat: string, itemId: string): Promise<Question[]> {
  try {
    const mod = await import(`@data/quiz/questions/quiz-questions-${cat}.js`);
    return mod.QUESTION_BANK?.[cat]?.[itemId] ?? [];
  } catch { return []; }
}
```

- [ ] **Step 4.2.3: learn.ts**

```ts
// src/data/learn.ts
export async function loadLearnContent(cat: string, itemId: string): Promise<string> {
  try {
    const resp = await fetch(`/data/learn-deep/${cat}/${getQuadrant(cat)}/${itemId}.md`);
    return await resp.text();
  } catch { return '# 内容加载失败'; }
}
function getQuadrant(cat: string) { return 'language'; /* 简化 */ }
```

- [ ] **Step 4.2.4: 验证数据加载**

写个测试页加载 C 语言元数据，运行验证。

- [ ] **Step 4.2.5: 提交**

```bash
git add engineering/apps/web/reading-radar/src/data
git commit -m "feat(reading-radar): data layer for tech/questions/learn"
```

---

## Task 4.3: Server.js → Express TypeScript 迁移

**Files:**
- Create: `D:\code\book\engineering\apps\web\reading-radar\server\index.ts`
- Create: `D:\code\book\engineering\apps\web\reading-radar\server\api\state.ts`
- Create: `D:\code\book\engineering\apps\web\reading-radar\server\storage\jsonStore.ts`

- [ ] **Step 4.3.1: 实现 jsonStore**

```ts
// server/storage/jsonStore.ts
import { promises as fs } from 'fs';
import path from 'path';

const DATA_DIR = path.resolve(__dirname, '../../user-data/state');

export async function readState<T>(key: string, fallback: T): Promise<T> {
  try {
    const data = await fs.readFile(path.join(DATA_DIR, `${key}.json`), 'utf-8');
    return JSON.parse(data);
  } catch { return fallback; }
}

export async function writeState<T>(key: string, value: T): Promise<void> {
  await fs.mkdir(DATA_DIR, { recursive: true });
  await fs.writeFile(path.join(DATA_DIR, `${key}.json`), JSON.stringify(value, null, 2));
}
```

- [ ] **Step 4.3.2: 实现 state API**

```ts
// server/api/state.ts
import { Router } from 'express';
import { readState, writeState } from '../storage/jsonStore';

export const stateRouter = Router();

stateRouter.get('/:key', async (req, res) => {
  const value = await readState(req.params.key, null);
  res.json(value);
});

stateRouter.put('/:key', async (req, res) => {
  await writeState(req.params.key, req.body);
  res.json({ ok: true });
});
```

- [ ] **Step 4.3.3: server/index.ts**

```ts
// server/index.ts
import express from 'express';
import { stateRouter } from './api/state';

const app = express();
app.use(express.json({ limit: '10mb' }));
app.use('/api/state', stateRouter);

const PORT = process.env.PORT ?? 8080;
app.listen(PORT, () => console.log(`Server on http://localhost:${PORT}`));
```

- [ ] **Step 4.3.4: 运行验证**

Run: `npm run server:dev`
Expected: Express 启动，可 `curl localhost:8080/api/state/test` 获取数据

- [ ] **Step 4.3.5: 提交**

```bash
git add engineering/apps/web/reading-radar/server
git commit -m "feat(reading-radar): migrate server.js to express + ts"
```

---

# MVP-4 完成验证

- [ ] React 项目能启动
- [ ] 能读取旧数据（tech/questions/learn）
- [ ] Express 服务运行，提供 state API

🎉 **MVP-4 完成**。读书雷达基础设施就绪。

---

# MVP-5: 读书雷达核心页迁移 (W10-W12)

> **Demo 能力**：Quiz / Learn / Kanban / Dashboard 四个核心页面迁移完成。

## Task 5.1: 路由 + Layout

**Files:**
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\router.tsx`
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\components\Layout.tsx`
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\App.tsx`

- [ ] **Step 5.1.1: Layout 组件**

```tsx
// src/components/Layout.tsx
import { Link, Outlet } from 'react-router-dom';

export function Layout() {
  return (
    <div className="min-h-screen flex flex-col">
      <header className="bg-white dark:bg-gray-800 border-b px-6 py-3">
        <div className="flex items-center gap-6">
          <Link to="/" className="font-bold text-xl">📚 Reading Radar</Link>
          <nav className="flex gap-4 text-sm">
            <Link to="/quiz">测评</Link>
            <Link to="/learn">学习</Link>
            <Link to="/kanban">看板</Link>
            <Link to="/dashboard">仪表盘</Link>
          </nav>
        </div>
      </header>
      <main className="flex-1 p-6"><Outlet /></main>
    </div>
  );
}
```

- [ ] **Step 5.1.2: router.tsx**

```tsx
// src/router.tsx
import { createBrowserRouter, RouterProvider } from 'react-router-dom';
import { Layout } from './components/Layout';
import { Home } from './pages/Home';
import { Quiz } from './pages/Quiz';
import { Learn } from './pages/Learn';
import { Kanban } from './pages/Kanban';
import { Dashboard } from './pages/Dashboard';

const router = createBrowserRouter([
  {
    element: <Layout />,
    children: [
      { path: '/', element: <Home /> },
      { path: '/quiz', element: <Quiz /> },
      { path: '/learn', element: <Learn /> },
      { path: '/learn/:cat/:item', element: <Learn /> },
      { path: '/kanban', element: <Kanban /> },
      { path: '/dashboard', element: <Dashboard /> },
    ],
  },
]);

export function Router() { return <RouterProvider router={router} />; }
```

- [ ] **Step 5.1.3: 各页占位组件**（先返回"开发中"提示）

- [ ] **Step 5.1.4: 提交**

```bash
git add engineering/apps/web/reading-radar/src
git commit -m "feat(reading-radar): layout and router setup"
```

---

## Task 5.2: Quiz 页面（最大难点）

**Files:**
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\pages\Quiz\index.tsx`
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\components\Quiz\QuestionCard.tsx`
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\components\Quiz\ScorePanel.tsx`

- [ ] **Step 5.2.1: QuestionCard**

```tsx
// src/components/Quiz/QuestionCard.tsx
import { Card } from '@shared/ui/Card';
import { Markdown } from '@shared/components/Markdown';
import type { Question } from '@/data/questions';

export function QuestionCard({ q, onAnswer }: {
  q: Question; onAnswer: (choice: string) => void;
}) {
  return (
    <Card className="p-6">
      <div className="text-xs text-gray-500 mb-2">{q.type} · {q.difficulty}</div>
      <p className="text-sm text-gray-600 mb-4 italic">{q.scenario}</p>
      <Markdown content={q.stem} />
      {q.code && <pre className="bg-gray-100 dark:bg-gray-800 p-3 rounded my-3 overflow-x-auto"><code>{q.code}</code></pre>}
      <div className="space-y-2 mt-4">
        {q.options.map((opt, i) => (
          <button key={i}
            onClick={() => onAnswer(opt)}
            className="w-full text-left p-3 rounded border hover:bg-primary-50 dark:hover:bg-gray-700"
          >
            {opt}
          </button>
        ))}
      </div>
    </Card>
  );
}
```

- [ ] **Step 5.2.2: Quiz 页面**

```tsx
// src/pages/Quiz/index.tsx
import { useEffect, useState } from 'react';
import { useParams } from 'react-router-dom';
import { loadQuestions } from '@/data/questions';
import { QuestionCard } from '@/components/Quiz/QuestionCard';

export function Quiz() {
  const params = useParams();
  const [questions, setQuestions] = useState<Question[]>([]);
  const [idx, setIdx] = useState(0);
  const [score, setScore] = useState(0);

  useEffect(() => {
    if (params.cat && params.item) {
      loadQuestions(params.cat, params.item).then(setQuestions);
    }
  }, [params.cat, params.item]);

  if (!questions.length) return <p>加载中...</p>;
  const q = questions[idx];

  return (
    <div className="max-w-3xl mx-auto">
      <div className="flex justify-between mb-4">
        <span>第 {idx + 1} / {questions.length} 题</span>
        <span>得分：{score}</span>
      </div>
      <QuestionCard q={q} onAnswer={(c) => {
        if (c.startsWith(q.answer as string)) setScore(s => s + 10);
        if (idx < questions.length - 1) setIdx(i => i + 1);
      }} />
    </div>
  );
}
```

- [ ] **Step 5.2.3: 测试 Quiz 页面**

Run: `npm run dev`，访问 `/quiz/c/pointer`，验证题目加载和答题流程

- [ ] **Step 5.2.4: 提交**

```bash
git add engineering/apps/web/reading-radar/src/pages/Quiz engineering/apps/web/reading-radar/src/components/Quiz
git commit -m "feat(reading-radar): quiz page with question card"
```

---

## Task 5.3: Learn 页面（Markdown 渲染）

**Files:**
- Create: `D:\code\book\engineering\apps\web\reading-radar\shared\web\src\components\Markdown.tsx`
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\pages\Learn\index.tsx`

- [ ] **Step 5.3.1: Markdown 组件**

```tsx
// shared/web/src/components/Markdown.tsx
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';

export function Markdown({ content }: { content: string }) {
  return (
    <article className="prose dark:prose-invert max-w-none">
      <ReactMarkdown remarkPlugins={[remarkGfm]}>{content}</ReactMarkdown>
    </article>
  );
}
```

- [ ] **Step 5.3.2: Learn 页面**

```tsx
// src/pages/Learn/index.tsx
import { useEffect, useState } from 'react';
import { useParams } from 'react-router-dom';
import { loadLearnContent } from '@/data/learn';
import { Markdown } from '@shared/components/Markdown';

export function Learn() {
  const { cat, item } = useParams();
  const [content, setContent] = useState('');

  useEffect(() => {
    if (cat && item) loadLearnContent(cat, item).then(setContent);
  }, [cat, item]);

  return (
    <div className="max-w-4xl mx-auto">
      <Markdown content={content} />
      <a href={`/quiz/${cat}/${item}`} className="mt-4 inline-block bg-primary-500 text-white px-4 py-2 rounded">
        🎯 去做测验
      </a>
    </div>
  );
}
```

- [ ] **Step 5.3.3: 验证**

访问 `/learn/c/pointer`，验证 MD 渲染

- [ ] **Step 5.3.4: 提交**

```bash
git add engineering/apps/web/reading-radar/shared engineering/apps/web/reading-radar/src/pages/Learn
git commit -m "feat(reading-radar): learn page with markdown rendering"
```

---

## Task 5.4: Kanban + Dashboard

（结构与 Quiz 类似，省略完整代码，每个页面是 1 个 commit）

**Files:**
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\pages\Kanban\index.tsx`
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\pages\Dashboard\index.tsx`
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\pages\Home\index.tsx`

- [ ] **Step 5.4.1: Kanban - 卡片网格 + 拖拽**

```tsx
// src/pages/Kanban/index.tsx
import { useState } from 'react';
import { loadTechMeta } from '@/data/tech';
import { Link } from 'react-router-dom';

export function Kanban() {
  const [items, setItems] = useState<any[]>([]);
  useEffect(() => { loadTechMeta('c').then(setItems); }, []);
  return (
    <div className="grid grid-cols-1 md:grid-cols-3 lg:grid-cols-4 gap-4">
      {items.map(item => (
        <div key={item.id} className="p-4 bg-white dark:bg-gray-800 rounded shadow">
          <h3 className="font-bold">{item.title}</h3>
          <p className="text-sm text-gray-500 mt-1">{item.desc}</p>
          <div className="mt-2 flex gap-2">
            <Link to={`/learn/c/${item.id}`} className="text-xs text-primary-500">📚 学习</Link>
            <Link to={`/quiz/c/${item.id}`} className="text-xs text-primary-500">🎯 测验</Link>
          </div>
        </div>
      ))}
    </div>
  );
}
```

- [ ] **Step 5.4.2: Dashboard - 掌握率 + 热力图**

```tsx
// src/pages/Dashboard/index.tsx
export function Dashboard() {
  // 简化：占位
  return (
    <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
      <div className="p-4 bg-white dark:bg-gray-800 rounded">掌握率: 65%</div>
      <div className="p-4 bg-white dark:bg-gray-800 rounded">最近测验: 92 分</div>
      <div className="p-4 bg-white dark:bg-gray-800 rounded">连续打卡: 7 天</div>
    </div>
  );
}
```

- [ ] **Step 5.4.3: 验证四个页面都能访问**

- [ ] **Step 5.4.4: 提交**

```bash
git add engineering/apps/web/reading-radar/src/pages
git commit -m "feat(reading-radar): kanban and dashboard pages"
```

---

# MVP-5 完成验证

- [ ] Quiz 能加载题目、答题、计分
- [ ] Learn 能渲染 Markdown + 跳测验
- [ ] Kanban 显示技术栈卡片 + 跳转
- [ ] Dashboard 显示汇总统计

🎉 **MVP-5 完成**。核心 4 页迁移完成。

---

# MVP-6: 次要页 + 测试 + 文档 (W13-W14)

> **Demo 能力**：所有页面完成 + 测试覆盖 + 文档齐全 + 部署配置。

## Task 6.1: 次要页面迁移

**Files:**
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\pages\FiveYearPlan\index.tsx`
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\pages\Interview\index.tsx`
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\pages\InterviewTracker\index.tsx`
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\pages\Practice\index.tsx`
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\pages\Grok\index.tsx`
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\pages\Excerpt\index.tsx`
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\pages\Home\index.tsx`（雷达图）

- [ ] **Step 6.1.1: 雷达图（Home）**

用 `<canvas>` 或简单 SVG 渲染知识雷达。

- [ ] **Step 6.1.2: FiveYearPlan - 日历 + 卡片**

- [ ] **Step 6.1.3: Interview / InterviewTracker - 面试题列表**

- [ ] **Step 6.1.4: Practice / Grok / Excerpt - 各自简化实现**

- [ ] **Step 6.1.5: 提交**

```bash
git add engineering/apps/web/reading-radar/src/pages
git commit -m "feat(reading-radar): migrate remaining pages"
```

---

## Task 6.2: 全局搜索

**Files:**
- Create: `D:\code\book\engineering\apps\web\reading-radar\src\components\GlobalSearch.tsx`

- [ ] **Step 6.2.1: 实现全局搜索（融合 tech/questions/learn）**

- [ ] **Step 6.2.2: 在 Layout 添加搜索框**

- [ ] **Step 6.2.3: 提交**

```bash
git add engineering/apps/web/reading-radar/src/components/GlobalSearch.tsx
git commit -m "feat(reading-radar): global search across tech/questions/learn"
```

---

## Task 6.3: 测试 + 覆盖率

**Files:**
- Create: `D:\code\book\engineering\apps\web\reading-radar\tests\unit\data\tech.test.ts`
- Create: `D:\code\book\engineering\apps\web\reading-radar\playwright.config.ts`

- [ ] **Step 6.3.1: 单元测试 - 数据层**

```ts
// tests/unit/data/tech.test.ts
import { describe, it, expect } from 'vitest';
import { loadTechMeta } from '@/data/tech';

describe('tech data', () => {
  it('loads C tech items', async () => {
    const items = await loadTechMeta('c');
    expect(items.length).toBeGreaterThan(0);
    expect(items[0]).toHaveProperty('id');
    expect(items[0]).toHaveProperty('title');
  });
});
```

- [ ] **Step 6.3.2: 覆盖率报告**

Run: `npm test -- --coverage`
Expected: 覆盖率 > 70%

- [ ] **Step 6.3.3: 提交**

```bash
git add engineering/apps/web/reading-radar/tests
git commit -m "test(reading-radar): add unit tests for data layer"
```

---

## Task 6.4: 文档

**Files:**
- Create: `D:\code\book\engineering\apps\web\reading-radar\README.md`
- Create: `D:\code\book\engineering\apps\web\reading-radar\docs\DEVELOPMENT.md`
- Create: `D:\code\book\engineering\apps\web\reading-radar\docs\MIGRATION.md`

- [ ] **Step 6.4.1: README.md**（覆盖：项目说明、技术栈、快速开始、命令）

- [ ] **Step 6.4.2: DEVELOPMENT.md**（开发指南：添加新页面、修改数据）

- [ ] **Step 6.4.3: MIGRATION.md**（从老 HTML 迁移指南、数据兼容说明）

- [ ] **Step 6.4.4: 提交**

```bash
git add engineering/apps/web/reading-radar/README.md engineering/apps/web/reading-radar/docs
git commit -m "docs(reading-radar): add README and migration guide"
```

---

## Task 6.5: 部署配置

**Files:**
- Create: `D:\code\book\engineering\apps\web\reading-radar\.github\workflows\deploy.yml`
- Create: `D:\code\book\engineering\apps\web\reading-radar\Dockerfile`

- [ ] **Step 6.5.1: GitHub Actions**

```yaml
name: Build & Test
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with: { node-version: '20' }
      - run: npm ci
      - run: npm test
      - run: npm run build
```

- [ ] **Step 6.5.2: Dockerfile（Nginx + dist）**

```dockerfile
FROM node:20-alpine AS build
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY . .
RUN npm run build

FROM nginx:alpine
COPY --from=build /app/dist /usr/share/nginx/html
COPY nginx.conf /etc/nginx/conf.d/default.conf
```

- [ ] **Step 6.5.3: 提交**

```bash
git add engineering/apps/web/reading-radar/.github engineering/apps/web/reading-radar/Dockerfile
git commit -m "ci(reading-radar): add github actions and docker"
```

---

# MVP-6 完成验证

- [ ] 11 个原 HTML 全部有对应 React 页面
- [ ] 全局搜索工作
- [ ] 测试覆盖率 > 70%
- [ ] 文档完整
- [ ] CI + Docker 就绪

🎉 **MVP-6 完成**。读书雷达改造完成。

---

# 全项目完成验证

- [ ] 两个项目都能 `npm run dev` 启动
- [ ] 两个项目都用同一套设计系统（视觉一致）
- [ ] 游戏 web 三个游戏都有：动画、撤销/笔记、成就、暗色、响应式
- [ ] 读书雷达 11 个页面都有对应 React 版本
- [ ] 旧数据（tech/questions/learn/user-data）兼容
- [ ] 测试覆盖率达标
- [ ] 文档齐全

## 交付物清单

- [ ] 完整源代码（两个项目）
- [ ] 编译产物（WASM）
- [ ] README + DEVELOPMENT + MIGRATION 文档
- [ ] 测试报告 + 覆盖率报告
- [ ] 部署配置（CI + Docker）
- [ ] 演示截图

## 回滚方案

旧 HTML 文件在过渡期保留，新版 React 应用顶部加横幅提示用户迁移。所有改动通过 git commit，可一键 revert 到旧版本。