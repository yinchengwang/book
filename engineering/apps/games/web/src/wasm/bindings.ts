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

export type { GameExports };