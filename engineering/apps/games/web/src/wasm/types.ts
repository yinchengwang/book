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