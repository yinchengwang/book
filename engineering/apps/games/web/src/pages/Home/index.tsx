// src/pages/Home/index.tsx
import { Link } from 'react-router-dom';
import { ThemeSwitcher } from '@/components/ThemeSwitcher';
import { LanguageSwitcher } from '@/components/LanguageSwitcher';

export function Home() {
  return (
    <div className="min-h-screen bg-gray-50 dark:bg-gray-900 p-8">
      <header className="flex items-center justify-between max-w-4xl mx-auto mb-8">
        <h1 className="text-4xl font-bold">🎮 游戏中心</h1>
        <div className="flex gap-2">
          <ThemeSwitcher />
          <LanguageSwitcher />
        </div>
      </header>
      <div className="grid grid-cols-1 md:grid-cols-3 gap-6 max-w-4xl mx-auto">
        <GameCard title="2048" desc="滑动合并" path="/2048" />
        <GameCard title="贪吃蛇" desc="控制蛇身" path="/snake" />
        <GameCard title="数独" desc="逻辑推理" path="/sudoku" />
      </div>
    </div>
  );
}

function GameCard({ title, desc, path, disabled }: {
  title: string; desc: string; path: string; disabled?: boolean;
}) {
  if (disabled) {
    return (
      <div className="p-6 bg-gray-100 dark:bg-gray-800 rounded-lg opacity-50 cursor-not-allowed">
        <h2 className="text-2xl font-bold">{title}</h2>
        <p className="text-gray-500">{desc}</p>
        <p className="text-sm text-gray-400 mt-2">敬请期待</p>
      </div>
    );
  }
  return (
    <Link
      to={path}
      className="p-6 bg-white dark:bg-gray-800 rounded-lg shadow hover:shadow-lg transition-shadow block"
    >
      <h2 className="text-2xl font-bold">{title}</h2>
      <p className="text-gray-500">{desc}</p>
    </Link>
  );
}
