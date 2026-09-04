// src/pages/Game2048/index.tsx
import { useEffect, useRef } from 'react';
import { useG2048 } from '@/games/g2048/useG2048';
import { renderBoard } from '@/games/g2048/renderer';
import { Button } from '@shared/ui/Button';

export function Game2048() {
  const { board, error, newGame, move } = useG2048();
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    if (!board || !canvasRef.current) return;
    const ctx = canvasRef.current.getContext('2d');
    if (!ctx) return;
    renderBoard(ctx, board.tiles);
  }, [board]);

  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      const map: Record<string, 0 | 1 | 2 | 3> = {
        ArrowUp: 0, ArrowDown: 1, ArrowLeft: 2, ArrowRight: 3,
        w: 0, s: 1, a: 2, d: 3, W: 0, S: 1, A: 2, D: 3,
      };
      const dir = map[e.key];
      if (dir !== undefined) {
        e.preventDefault();
        move(dir);
      }
      if (e.key === 'r' || e.key === 'R') {
        newGame();
      }
    };
    document.addEventListener('keydown', handler);
    return () => document.removeEventListener('keydown', handler);
  }, [move, newGame]);

  if (error) {
    return (
      <div className="p-8 text-center">
        <p className="text-red-500">WASM 加载失败：{error}</p>
        <p className="text-sm text-gray-500 mt-2">运行 npm run wasm:build 重新编译</p>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-g2048-bg dark:bg-gray-900 p-8">
      <header className="flex items-center justify-between max-w-md mx-auto mb-4">
        <a
          href="/"
          className="text-sm text-gray-600 dark:text-gray-300 hover:underline"
        >
          ← 返回首页
        </a>
        <h1 className="text-3xl font-bold text-gray-700 dark:text-white">2048</h1>
        <div className="w-16" />
      </header>
      <div className="text-center mb-4 text-gray-700 dark:text-gray-200">
        分数：<span className="font-bold" data-testid="score">{board?.score ?? 0}</span>
        {board?.gameOver && (
          <span className="ml-4 text-red-500 font-bold">游戏结束</span>
        )}
        {board?.won && (
          <span className="ml-4 text-yellow-500 font-bold">🎉 达成 2048！</span>
        )}
      </div>
      <canvas
        ref={canvasRef}
        width={400}
        height={400}
        className="mx-auto rounded-lg shadow-lg block"
      />
      <div className="text-center mt-4">
        <Button onClick={() => newGame()}>新游戏 (R)</Button>
      </div>
      <p className="text-center text-sm text-gray-500 dark:text-gray-400 mt-4">
        WASD 或方向键移动 · R 重新开始
      </p>
    </div>
  );
}
