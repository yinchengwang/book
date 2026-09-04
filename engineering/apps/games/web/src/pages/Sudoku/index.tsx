// src/pages/Sudoku/index.tsx
import { useState, useEffect } from 'react';
import { Link } from 'react-router-dom';
import { useSudoku } from '@/games/sudoku/useSudoku';
import { Button } from '@shared/ui/Button';

type Difficulty = 0 | 1 | 2;

export function Sudoku() {
  const [difficulty, setDifficulty] = useState<Difficulty>(0);
  const [selected, setSelected] = useState<[number, number] | null>(null);
  const { board, newGame, setCell, eraseCell } = useSudoku(difficulty);

  // 键盘输入 1-9 + 0/Backspace/Delete 删除，Esc 取消选中
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if (!selected || !board) return;
      const [r, c] = selected;
      const n = parseInt(e.key, 10);
      if (n >= 1 && n <= 9) {
        e.preventDefault();
        setCell(r, c, n);
      } else if (e.key === '0' || e.key === 'Backspace' || e.key === 'Delete') {
        e.preventDefault();
        eraseCell(r, c);
      } else if (e.key === 'Escape') {
        setSelected(null);
      }
    };
    document.addEventListener('keydown', handler);
    return () => document.removeEventListener('keydown', handler);
  }, [selected, board, setCell, eraseCell]);

  return (
    <div className="min-h-screen bg-gray-50 dark:bg-gray-900 p-8 text-center">
      <header className="flex justify-between items-center mb-4 max-w-md mx-auto">
        <Link to="/" className="text-sm text-primary-500 hover:underline">
          ← 返回首页
        </Link>
        <h1 className="text-3xl font-bold">🔢 数独</h1>
        <Button onClick={newGame} variant="primary" size="sm">
          新游戏
        </Button>
      </header>

      <div className="mb-4 flex justify-center gap-2">
        {(['简单', '中等', '困难'] as const).map((label, i) => (
          <Button
            key={i}
            variant={difficulty === i ? 'primary' : 'ghost'}
            onClick={() => setDifficulty(i as Difficulty)}
          >
            {label}
          </Button>
        ))}
      </div>

      {board?.over && (
        <p className="text-green-500 text-xl mb-4">🎉 完成！</p>
      )}

      {board && (
        <div className="inline-block border-4 border-sudoku-grid relative">
          {board.cells.map((row, r) => (
            <div key={r} className="flex">
              {row.map((cell, c) => {
                const isSelected =
                  selected !== null && selected[0] === r && selected[1] === c;
                const rightThick = c === 2 || c === 5;
                const bottomThick = r === 2 || r === 5;
                return (
                  <button
                    key={c}
                    type="button"
                    data-testid={`cell-${r}-${c}`}
                    className={`w-12 h-12 border border-gray-300 text-lg font-bold transition-colors
                      ${rightThick ? 'border-r-4 border-r-sudoku-grid' : ''}
                      ${bottomThick ? 'border-b-4 border-b-sudoku-grid' : ''}
                      ${
                        cell.given
                          ? 'text-gray-900 bg-gray-100 dark:bg-gray-700 dark:text-gray-100 cursor-not-allowed'
                          : 'text-primary-500 bg-white dark:bg-gray-800 hover:bg-blue-50 dark:hover:bg-gray-700'
                      }
                      ${cell.conflict ? '!text-red-500 !bg-red-50 dark:!bg-red-900/30' : ''}
                      ${isSelected ? 'ring-2 ring-primary-500' : ''}`}
                    onClick={() => {
                      if (!cell.given) setSelected([r, c]);
                    }}
                    disabled={cell.given}
                  >
                    {cell.value !== 0 ? cell.value : ''}
                  </button>
                );
              })}
            </div>
          ))}
          {board.over && (
            <div className="absolute inset-0 flex items-center justify-center bg-black/40">
              <div className="bg-white dark:bg-gray-800 px-6 py-4 rounded-lg shadow-lg">
                <p className="text-2xl font-bold mb-2 text-green-500">🎉 完成！</p>
                <Button onClick={newGame}>新游戏</Button>
              </div>
            </div>
          )}
        </div>
      )}

      <p className="mt-4 text-sm text-gray-500">
        点击格子选中，按 1-9 填数，按 0/Backspace 删除，Esc 取消选中
      </p>
    </div>
  );
}