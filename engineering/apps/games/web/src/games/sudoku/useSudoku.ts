// src/games/sudoku/useSudoku.ts
import { useEffect, useState, useCallback } from 'react';
import { sudoku } from '@/wasm/bindings';
import type { Board, Cell } from './types';

export function useSudoku(difficulty: 0 | 1 | 2 = 0) {
  const [board, setBoard] = useState<Board | null>(null);

  const refresh = useCallback(async () => {
    const cells: Cell[][] = [];
    for (let r = 0; r < 9; r++) {
      cells[r] = [];
      for (let c = 0; c < 9; c++) {
        cells[r][c] = {
          value: await sudoku.value(r, c),
          given: await sudoku.given(r, c),
          conflict: await sudoku.conflict(r, c),
          notes: await sudoku.notes(r, c),
        };
      }
    }
    setBoard({ cells, over: await sudoku.over(), difficulty });
  }, [difficulty]);

  const newGame = useCallback(async () => {
    await sudoku.init(difficulty, Date.now());
    await refresh();
  }, [difficulty, refresh]);

  const setCell = useCallback(
    async (r: number, c: number, n: number) => {
      await sudoku.set(r, c, n);
      await refresh();
    },
    [refresh]
  );

  const eraseCell = useCallback(
    async (r: number, c: number) => {
      await sudoku.erase(r, c);
      await refresh();
    },
    [refresh]
  );

  const toggleNote = useCallback(
    async (r: number, c: number, n: number) => {
      await sudoku.toggleNote(r, c, n);
      await refresh();
    },
    [refresh]
  );

  // 智能提示：揭示 (r,c) 处的正确答案（经 sudoku.set 填入，保持冲突/终局状态一致）
  const hintCell = useCallback(
    async (r: number, c: number) => {
      const n = await sudoku.hint(r, c);
      if (n >= 1 && n <= 9) {
        await sudoku.set(r, c, n);
        await refresh();
      }
    },
    [refresh]
  );

  // 候选数：isValid 过滤 1-9（唯余法视角的合法候选）
  const validCandidates = useCallback(async (r: number, c: number): Promise<number[]> => {
    const out: number[] = [];
    for (let n = 1; n <= 9; n++) {
      if (await sudoku.isValid(r, c, n)) out.push(n);
    }
    return out;
  }, []);

  useEffect(() => {
    newGame();
  }, [newGame]);

  return { board, newGame, setCell, eraseCell, toggleNote, hintCell, validCandidates };
}
