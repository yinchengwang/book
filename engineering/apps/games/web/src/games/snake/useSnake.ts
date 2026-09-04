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
    snake.init(Date.now(), difficulty).then(() => {
      rafRef.current = requestAnimationFrame(loop);
    });
    return () => {
      if (rafRef.current) cancelAnimationFrame(rafRef.current);
    };
  }, [difficulty, tick]);

  return state;
}