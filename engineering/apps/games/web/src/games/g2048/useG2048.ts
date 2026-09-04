// src/games/g2048/useG2048.ts
import { useState, useEffect, useCallback, useRef } from 'react';
import { g2048 } from '@/wasm/bindings';
import type { Board, Direction } from './types';
import { History, type Snapshot } from './history';

export function useG2048() {
  const [board, setBoard] = useState<Board | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [canUndo, setCanUndo] = useState(false);
  const historyRef = useRef<History>(new History(20));

  const newGame = useCallback(async (seed?: number) => {
    try {
      historyRef.current.clear();
      setCanUndo(false);
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
    const next: Board = {
      tiles,
      score: await g2048.score(),
      gameOver: await g2048.gameOver(),
      won: await g2048.won(),
    };
    setBoard(next);
    return next;
  }, []);

  const captureSnapshot = useCallback(async (): Promise<Snapshot | null> => {
    const current = board;
    if (!current) return null;
    return {
      tiles: current.tiles.map((row) => row.slice()),
      score: current.score,
    };
  }, [board]);

  const move = useCallback(async (dir: Direction) => {
    const snap = await captureSnapshot();
    const moved = await g2048.move(dir);
    if (moved && snap) {
      historyRef.current.push(snap);
      setCanUndo(true);
      await refresh();
    }
    return moved;
  }, [captureSnapshot, refresh]);

  const undo = useCallback(async () => {
    const snap = historyRef.current.pop();
    if (!snap) {
      setCanUndo(false);
      return false;
    }
    const flat: number[] = [];
    for (let r = 0; r < 4; r++) {
      for (let c = 0; c < 4; c++) {
        flat.push(snap.tiles[r][c]);
      }
    }
    await g2048.setBoard(flat, snap.score);
    await refresh();
    setCanUndo(historyRef.current.canUndo);
    return true;
  }, [refresh]);

  useEffect(() => { newGame(); }, [newGame]);

  return { board, error, newGame, move, undo, canUndo };
}
