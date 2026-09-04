// src/games/snake/useSnake.ts
import { useEffect, useRef, useState, useCallback } from 'react';
import { snake } from '@/wasm/bindings';
import type { SnakeState } from './types';

export function useSnake(difficulty: 0 | 1 | 2 = 0): SnakeState {
  const [state, setState] = useState<SnakeState>({
    score: 0,
    over: false,
    len: 0,
    bodyX: [],
    bodyY: [],
    foodX: 0,
    foodY: 0,
  });
  const rafRef = useRef<number>();

  const tick = useCallback(async () => {
    await snake.tick();
    const len = await snake.len();
    const bodyX: number[] = new Array(len);
    const bodyY: number[] = new Array(len);
    for (let i = 0; i < len; i++) {
      bodyX[i] = await snake.bodyX(i);
      bodyY[i] = await snake.bodyY(i);
    }
    setState({
      score: await snake.score(),
      over: await snake.over(),
      len,
      bodyX,
      bodyY,
      foodX: await snake.foodX(),
      foodY: await snake.foodY(),
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
    snake.init(Date.now(), difficulty).then(() => {
      rafRef.current = requestAnimationFrame(loop);
    });
    return () => {
      if (rafRef.current) cancelAnimationFrame(rafRef.current);
    };
  }, [difficulty, tick]);

  return state;
}