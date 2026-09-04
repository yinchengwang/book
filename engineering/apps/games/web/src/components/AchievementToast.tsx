// src/components/AchievementToast.tsx
import { useEffect } from 'react';
import { useAchievements } from '@/stores/achievements';

const TOAST_DURATION_MS = 3000;

export function AchievementToast() {
  const toast = useAchievements((s) => s.toast);
  const dismiss = useAchievements((s) => s.dismissToast);

  useEffect(() => {
    if (!toast) return;
    const t = setTimeout(dismiss, TOAST_DURATION_MS);
    return () => clearTimeout(t);
  }, [toast, dismiss]);

  if (!toast) return null;

  return (
    <div
      role="status"
      aria-live="polite"
      className="fixed bottom-4 right-4 z-50 tile-pop"
    >
      <div className="bg-primary-500 text-white px-4 py-2 rounded-lg shadow-lg flex items-center gap-2">
        <span aria-hidden="true">🏆</span>
        <span className="font-medium">{toast}</span>
      </div>
    </div>
  );
}
