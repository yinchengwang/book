// src/components/ThemeSwitcher.tsx
import { useTheme } from '@shared/theme/ThemeProvider';
import { Button } from '@shared/ui/Button';

export function ThemeSwitcher() {
  const { theme, toggle } = useTheme();
  return (
    <Button
      variant="ghost"
      size="sm"
      onClick={toggle}
      aria-label="Toggle theme"
    >
      {theme === 'dark' ? '☀️' : '🌙'}
    </Button>
  );
}
