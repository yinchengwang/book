// src/games/snake/types.ts
export interface SnakeState {
  score: number;
  over: boolean;
  len: number;
  bodyX: number[];
  bodyY: number[];
  foodX: number;
  foodY: number;
}