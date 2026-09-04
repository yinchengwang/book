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

export const snake = {
  async init(seed: number, difficulty = 0) {
    const m = await loadWasm();
    m._snake_init_js(seed, difficulty);
  },
  async tick() {
    const m = await loadWasm();
    m._snake_tick_js();
  },
  async input(dir: 0 | 1 | 2 | 3) {
    const m = await loadWasm();
    m._snake_input_js(dir);
  },
  async len(): Promise<number> {
    const m = await loadWasm();
    return m._snake_len_js();
  },
  async bodyX(i: number): Promise<number> {
    const m = await loadWasm();
    return m._snake_body_x_js(i);
  },
  async bodyY(i: number): Promise<number> {
    const m = await loadWasm();
    return m._snake_body_y_js(i);
  },
  async foodX(): Promise<number> {
    const m = await loadWasm();
    return m._snake_food_x_js();
  },
  async foodY(): Promise<number> {
    const m = await loadWasm();
    return m._snake_food_y_js();
  },
  async score(): Promise<number> {
    const m = await loadWasm();
    return m._snake_score_js();
  },
  async over(): Promise<boolean> {
    const m = await loadWasm();
    return m._snake_over_js() === 1;
  },
};

export type { GameExports };

export const sudoku = {
  async init(d: 0 | 1 | 2, seed: number) {
    const m = await loadWasm();
    m._sudoku_init_js(d, seed);
  },
  async set(r: number, c: number, n: number) {
    const m = await loadWasm();
    return m._sudoku_set_js(r, c, n);
  },
  async erase(r: number, c: number) {
    const m = await loadWasm();
    m._sudoku_erase_js(r, c);
  },
  async value(r: number, c: number): Promise<number> {
    const m = await loadWasm();
    return m._sudoku_value_js(r, c);
  },
  async given(r: number, c: number): Promise<boolean> {
    const m = await loadWasm();
    return m._sudoku_given_js(r, c) === 1;
  },
  async conflict(r: number, c: number): Promise<boolean> {
    const m = await loadWasm();
    return m._sudoku_conflict_js(r, c) === 1;
  },
  async over(): Promise<boolean> {
    const m = await loadWasm();
    return m._sudoku_over_js() === 1;
  },
};