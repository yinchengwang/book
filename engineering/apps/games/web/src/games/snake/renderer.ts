// src/games/snake/renderer.ts
import type { SnakeState } from './types';

export function renderSnake(
  ctx: CanvasRenderingContext2D,
  state: SnakeState,
  cellSize = 20
): void {
  const size = cellSize * 20;
  ctx.fillStyle = '#fff';
  ctx.fillRect(0, 0, size, size);
  ctx.fillStyle = '#2ecc71';
  for (let i = 0; i < state.len; i++) {
    ctx.fillRect(state.bodyX[i] * cellSize + 1, state.bodyY[i] * cellSize + 1, cellSize - 2, cellSize - 2);
  }
  ctx.fillStyle = '#e74c3c';
  ctx.fillRect(state.foodX * cellSize + 1, state.foodY * cellSize + 1, cellSize - 2, cellSize - 2);
}