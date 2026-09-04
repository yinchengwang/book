// tests/unit/wasm/binding.test.ts
import { describe, it, expect, beforeAll, vi } from 'vitest';
import { loadWasm, resetWasmCache } from '@/wasm/loader';

const mockExports = {
  _g2048_init_js: () => {},
  _g2048_move_js: () => 1,
  _g2048_tile_js: () => 0,
  _g2048_score_js: () => 0,
  _g2048_game_over_js: () => 0,
  _g2048_won_js: () => 0,
  _g2048_can_move_js: () => 1,
};

describe('WASM bindings', () => {
  beforeAll(() => {
    // 在 Node 环境模拟 window + GameModule
    (global as any).window = (global as any).window || {};
    (window as any).GameModule = async () => mockExports;
  });

  it('loadWasm resolves with exports', async () => {
    resetWasmCache();
    const m = await loadWasm();
    expect(m).toBeDefined();
    expect(typeof m._g2048_init_js).toBe('function');
    expect(m._g2048_move_js()).toBe(1);
  });
});