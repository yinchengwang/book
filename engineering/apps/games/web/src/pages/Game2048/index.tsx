// src/pages/Game2048/index.tsx
import { useEffect, useRef, useState, useCallback } from 'react';
import { Link } from 'react-router-dom';
import { useG2048 } from '@/games/g2048/useG2048';
import { renderBoard } from '@/games/g2048/renderer';
import { useSwipeHandlers } from '@/games/g2048/gesture';
import { Button } from '@shared/ui/Button';

const HIGH_SCORE_KEY = 'g2048_high_score';

function safeGetNumber(key: string, fallback: number): number {
  try {
    const raw = localStorage.getItem(key);
    if (raw === null) return fallback;
    const n = JSON.parse(raw);
    return typeof n === 'number' && Number.isFinite(n) ? n : fallback;
  } catch {
    return fallback;
  }
}

function safeSetNumber(key: string, value: number): void {
  try {
    localStorage.setItem(key, JSON.stringify(value));
  } catch {
    // ignore quota / disabled storage
  }
}

export function Game2048() {
  const { board, error, newGame, move, undo, canUndo } = useG2048();
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [highScore, setHighScore] = useState<number>(() =>
    safeGetNumber(HIGH_SCORE_KEY, 0)
  );

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || !board) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const dpr = window.devicePixelRatio || 1;
    const cssSize = 400;
    canvas.width = cssSize * dpr;
    canvas.height = cssSize * dpr;
    canvas.style.width = `${cssSize}px`;
    canvas.style.height = `${cssSize}px`;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    renderBoard(ctx, board.tiles);
  }, [board]);

  useEffect(() => {
    if (board && board.score > highScore) {
      setHighScore(board.score);
      safeSetNumber(HIGH_SCORE_KEY, board.score);
    }
  }, [board, highScore]);

  const handleNewGame = useCallback(() => {
    newGame();
  }, [newGame]);

  const swipeHandlers = useSwipeHandlers(useCallback((dir: 0 | 1 | 2 | 3) => {
    move(dir);
  }, [move]));

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
        handleNewGame();
      }
      if (e.key === 'u' || e.key === 'U') {
        e.preventDefault();
        undo();
      }
    };
    document.addEventListener('keydown', handler);
    return () => document.removeEventListener('keydown', handler);
  }, [move, handleNewGame, undo]);

  if (error) {
    return (
      <div className="p-8 text-center">
        <p className="text-red-500">WASM 加载失败：{error}</p>
        <p className="text-sm text-gray-500 mt-2">运行 npm run wasm:build 重新编译</p>
      </div>
    );
  }

  const showOverlay = board?.gameOver || board?.won;
  const overlayText = board?.won ? '恭喜达成 2048！' : '游戏结束';

  return (
    <div className="min-h-screen bg-g2048-bg dark:bg-gray-900 p-8">
      <header className="flex items-center justify-between max-w-md mx-auto mb-4">
        <Link
          to="/"
          className="text-sm text-gray-600 dark:text-gray-300 hover:underline"
        >
          ← 返回首页
        </Link>
        <h1 className="text-3xl font-bold text-gray-700 dark:text-white">2048</h1>
        <div className="w-16" />
      </header>
      <div className="flex items-center justify-center gap-6 mb-4 text-gray-700 dark:text-gray-200">
        <div>
          分数：<span className="font-bold" data-testid="score">{board?.score ?? 0}</span>
        </div>
        <div>
          最高：<span className="font-bold" data-testid="high-score">{highScore}</span>
        </div>
      </div>
      <div className="relative max-w-md mx-auto touch-none" {...swipeHandlers}>
        <canvas
          ref={canvasRef}
          className="mx-auto rounded-lg shadow-lg block"
        />
        {showOverlay && (
          <div className="absolute inset-0 flex items-center justify-center bg-black/40 rounded-lg">
            <div className="bg-white dark:bg-gray-800 rounded-lg shadow-xl p-6 text-center">
              <p className={`text-2xl font-bold mb-4 ${board?.won ? 'text-yellow-500' : 'text-red-500'}`}>
                {overlayText}
              </p>
              <Button onClick={handleNewGame}>新游戏</Button>
            </div>
          </div>
        )}
      </div>
      <div className="text-center mt-4 flex items-center justify-center gap-2">
        <Button onClick={handleNewGame}>新游戏 (R)</Button>
        <Button onClick={undo} disabled={!canUndo}>撤销 (U)</Button>
      </div>
      <p className="text-center text-sm text-gray-500 dark:text-gray-400 mt-4">
        WASD / 方向键 / 滑动移动 · R 重新开始 · U 撤销
      </p>
    </div>
  );
}
