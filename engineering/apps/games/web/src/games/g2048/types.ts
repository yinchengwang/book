// src/games/g2048/types.ts
export type Direction = 0 | 1 | 2 | 3;
export type Difficulty = 4 | 5 | 6; // 棋盘大小

export interface Board {
  tiles: number[][]; // 4x4, 5x5, 6x6
  score: number;
  gameOver: boolean;
  won: boolean;
}