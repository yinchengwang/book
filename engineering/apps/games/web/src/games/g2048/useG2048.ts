// src/games/g2048/useG2048.ts
import { useState, useEffect, useCallback } from 'react';
import { g2048 } from '@/wasm/bindings';
import type { Board, Direction } from './types';

export function useG2048() {
  const [board, setBoard] = useState<Board | null>(null);
  const [error, setError] = useState<string | null>(null);

  const newGame = useCallback(async (seed?: number) => {
    try {
      await g2048.init(seed ?? Date.now());
      await refresh();
    } catch (e) {
      setError((e as Error).message);
    }
  }, []);

  const refresh = useCallback(async () => {
    const size = 4;
    const tiles: number[][] = [];
    for (let r = 0; r < size; r++) {
      tiles[r] = [];
      for (let c = 0; c < size; c++) {
        tiles[r][c] = await g2048.tile(r, c);
      }
    }
    setBoard({
      tiles,
      score: await g2048.score(),
      gameOver: await g2048.gameOver(),
      won: await g2048.won(),
    });
  }, []);

  const move = useCallback(async (dir: Direction) => {
    const moved = await g2048.move(dir);
    if (moved) await refresh();
    return moved;
  }, [refresh]);

  useEffect(() => { newGame(); }, [newGame]);

  return { board, error, newGame, move };
}