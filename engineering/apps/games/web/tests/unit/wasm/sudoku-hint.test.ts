// tests/unit/wasm/sudoku-hint.test.ts
//
// 真实 WASM 链路测试：binding.c 的 sudoku_hint_js / sudoku_is_valid_js
// 经 Emscripten 模块在 Node 环境直接加载（games.js/games.wasm 同目录）。
import { describe, it, expect, beforeAll } from 'vitest';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const nodeRequire = createRequire(import.meta.url);
const dir = path.dirname(fileURLToPath(import.meta.url));
const wasmDir = path.resolve(dir, '../../../public/wasm');

interface SudokuModule {
  _sudoku_init_js(d: number, seed: number): void;
  _sudoku_set_js(r: number, c: number, n: number): number;
  _sudoku_value_js(r: number, c: number): number;
  _sudoku_given_js(r: number, c: number): number;
  _sudoku_hint_js(r: number, c: number): number;
  _sudoku_is_valid_js(r: number, c: number, n: number): number;
}

type Factory = (opts: { locateFile: (p: string) => string }) => Promise<SudokuModule>;

// 项目 package.json 为 "type": "module"，games.js 的 UMD 守卫
// （typeof exports==="object" && typeof module==="object"）在 ESM 语义下不成立，
// 直接 require 会得到空命名空间。这里手动提供 module/exports 上下文执行 UMD。
function loadFactory(): Factory {
  const code = readFileSync(path.join(wasmDir, 'games.js'), 'utf8');
  const mod = { exports: {} as Record<string, unknown> };
  new Function('module', 'exports', 'require', '__filename', '__dirname', code)(
    mod,
    mod.exports,
    nodeRequire,
    path.join(wasmDir, 'games.js'),
    wasmDir
  );
  return mod.exports as unknown as Factory;
}

function findEmptyCell(m: SudokuModule): [number, number] {
  for (let r = 0; r < 9; r++) {
    for (let c = 0; c < 9; c++) {
      if (!m._sudoku_given_js(r, c) && m._sudoku_value_js(r, c) === 0) return [r, c];
    }
  }
  throw new Error('no empty cell');
}

describe('sudoku hint/isValid（真实 WASM）', () => {
  let m: SudokuModule;

  beforeAll(async () => {
    m = await loadFactory()({ locateFile: (p) => path.join(wasmDir, p) });
  });

  it('hint 返回空格的正解，且为合法放置，set 后生效', () => {
    m._sudoku_init_js(0, 12345);
    const [r, c] = findEmptyCell(m);
    const hint = m._sudoku_hint_js(r, c);
    expect(hint).toBeGreaterThanOrEqual(1);
    expect(hint).toBeLessThanOrEqual(9);
    // 新局空格上，正解必为合法候选
    expect(m._sudoku_is_valid_js(r, c, hint)).toBe(1);
    // 经 set 填入（与 UI hintCell 路径一致）
    expect(m._sudoku_set_js(r, c, hint)).toBe(1);
    expect(m._sudoku_value_js(r, c)).toBe(hint);
  });

  it('isValid 拒绝同行已存在的数字', () => {
    m._sudoku_init_js(0, 12345);
    // 找一个 given 格，取同行空格验证其值被判非法
    for (let r = 0; r < 9; r++) {
      for (let c = 0; c < 9; c++) {
        if (!m._sudoku_given_js(r, c)) continue;
        const v = m._sudoku_value_js(r, c);
        for (let c2 = 0; c2 < 9; c2++) {
          if (!m._sudoku_given_js(r, c2) && m._sudoku_value_js(r, c2) === 0) {
            expect(m._sudoku_is_valid_js(r, c2, v)).toBe(0);
            return;
          }
        }
      }
    }
    throw new Error('no given/empty pair found in any row');
  });

  it('越界参数返回 0', () => {
    m._sudoku_init_js(0, 12345);
    expect(m._sudoku_hint_js(-1, 0)).toBe(0);
    expect(m._sudoku_hint_js(0, 9)).toBe(0);
    expect(m._sudoku_is_valid_js(0, 0, 0)).toBe(0);
    expect(m._sudoku_is_valid_js(0, 0, 10)).toBe(0);
  });
});
