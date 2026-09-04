// src/pages/Snake/index.tsx
import { useEffect, useRef, useState } from 'react';
import { Link } from 'react-router-dom';
import { useSnake } from '@/games/snake/useSnake';
import { snake } from '@/wasm/bindings';
import { renderSnake } from '@/games/snake/renderer';
import { Button } from '@shared/ui/Button';

type Difficulty = 0 | 1 | 2;

export function Snake() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [difficulty, setDifficulty] = useState<Difficulty>(0);
  const [highScore, setHighScore] = useState<number>(() => {
    try {
      const v = JSON.parse(localStorage.getItem('snake_high_score') ?? '0');
      return typeof v === 'number' && Number.isFinite(v) ? v : 0;
    } catch {
      return 0;
    }
  });
  const state = useSnake(difficulty);

  // Canvas 渲染（响应 state 变化）
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    const dpr = window.devicePixelRatio || 1;
    canvas.width = 400 * dpr;
    canvas.height = 400 * dpr;
    canvas.style.width = '400px';
    canvas.style.height = '400px';
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    renderSnake(ctx, state, 20);
  }, [state]);

  // 最高分持久化
  useEffect(() => {
    if (state.score > highScore) {
      setHighScore(state.score);
      try {
        localStorage.setItem('snake_high_score', JSON.stringify(state.score));
      } catch {
        // ignore quota / disabled storage
      }
    }
  }, [state.score, highScore]);

  // 键盘
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      const m: Record<string, 0 | 1 | 2 | 3> = {
        ArrowUp: 0, ArrowDown: 1, ArrowLeft: 2, ArrowRight: 3,
        w: 0, s: 1, a: 2, d: 3,
        W: 0, S: 1, A: 2, D: 3,
      };
      const d = m[e.key];
      if (d !== undefined) {
        e.preventDefault();
        snake.input(d);
      }
      if (e.key === 'r' || e.key === 'R') {
        location.reload();
      }
    };
    document.addEventListener('keydown', handler);
    return () => document.removeEventListener('keydown', handler);
  }, []);

  return (
    <div className="min-h-screen bg-snake-board dark:bg-gray-900 p-8 text-center">
      <header className="flex justify-between items-center mb-4 max-w-md mx-auto">
        <Link to="/" className="text-sm text-primary-500 hover:underline">
          ← 返回首页
        </Link>
        <h1 className="text-3xl font-bold">🐍 贪吃蛇</h1>
        <div className="text-right">
          <div>
            分数：<span className="font-bold" data-testid="score">{state.score}</span>
          </div>
          <div className="text-sm text-gray-500">最高：{highScore}</div>
        </div>
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

      <div className="relative mx-auto touch-none" style={{ width: 400, height: 400 }}>
        <canvas
          ref={canvasRef}
          className="border-2 border-gray-300 mx-auto block"
        />
        {state.over && (
          <div className="absolute inset-0 flex items-center justify-center bg-black/40 rounded-lg">
            <div className="bg-white dark:bg-gray-800 px-6 py-4 rounded-lg shadow-lg">
              <p className="text-xl font-bold mb-2 text-red-500">游戏结束</p>
              <Button onClick={() => location.reload()}>新游戏</Button>
            </div>
          </div>
        )}
      </div>

      <div className="mt-4 grid grid-cols-3 gap-2 max-w-[200px] mx-auto md:hidden">
        <span />
        <Button
          onTouchStart={(e) => {
            e.preventDefault();
            snake.input(0);
          }}
          size="lg"
          aria-label="上"
        >
          ↑
        </Button>
        <span />
        <Button
          onTouchStart={(e) => {
            e.preventDefault();
            snake.input(2);
          }}
          size="lg"
          aria-label="左"
        >
          ←
        </Button>
        <Button
          onTouchStart={(e) => {
            e.preventDefault();
            snake.input(1);
          }}
          size="lg"
          aria-label="下"
        >
          ↓
        </Button>
        <Button
          onTouchStart={(e) => {
            e.preventDefault();
            snake.input(3);
          }}
          size="lg"
          aria-label="右"
        >
          →
        </Button>
      </div>

      <p className="mt-4 text-sm text-gray-500">
        WASD / 方向键 / 触屏按钮移动 · R 重新开始
      </p>
    </div>
  );
}