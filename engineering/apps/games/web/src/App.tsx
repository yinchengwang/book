// src/App.tsx
import { Router } from './router';
import { AchievementToast } from './components/AchievementToast';

export function App() {
  return (
    <>
      <Router />
      <AchievementToast />
    </>
  );
}
