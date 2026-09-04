// src/games/sudoku/types.ts
export interface Cell {
  value: number;
  given: boolean;
  conflict: boolean;
  notes: number;
}

export interface Board {
  cells: Cell[][];
  over: boolean;
  difficulty: 0 | 1 | 2;
}
