// src/components/LanguageSwitcher.tsx
import { useTranslation } from 'react-i18next';
import { Button } from '@shared/ui/Button';

export function LanguageSwitcher() {
  const { i18n } = useTranslation();
  const next = i18n.language === 'zh' ? 'en' : 'zh';
  return (
    <Button
      variant="ghost"
      size="sm"
      onClick={() => {
        i18n.changeLanguage(next);
        try {
          localStorage.setItem('lang', next);
        } catch {
          /* localStorage unavailable (SSR/private mode); ignore */
        }
      }}
      aria-label="Toggle language"
    >
      {next.toUpperCase()}
    </Button>
  );
}
