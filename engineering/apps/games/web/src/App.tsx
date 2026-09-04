// src/App.tsx
import { useEffect } from 'react';
import { Button } from '@shared/ui/Button';
import { useTheme } from '@shared/theme/ThemeProvider';

export function App() {
  const { theme, toggle } = useTheme();
  useEffect(() => { console.log('GameModule:', !!window.GameModule); }, []);
  return (
    <div className="min-h-screen flex flex-col items-center justify-center gap-4">
      <h1 className="text-4xl font-bold">🎮 游戏中心</h1>
      <Button onClick={toggle}>切换到{theme === 'light' ? '暗色' : '亮色'}</Button>
    </div>
  );
}