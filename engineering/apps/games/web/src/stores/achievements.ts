// src/stores/achievements.ts
import { create } from 'zustand';
import { safeGet, safeSet } from '@shared/storage/safeStorage';

export interface Achievement {
  id: string;
  name: string;
  desc: string;
}

const ALL: Achievement[] = [
  { id: 'first-2048', name: '初识 2048', desc: '第一次玩 2048' },
  { id: 'merge-128', name: '合并达人', desc: '合并到 128' },
  { id: 'snake-50', name: '蛇中豪杰', desc: '贪吃蛇吃到 50 分' },
  { id: 'sudoku-easy', name: '数独新手', desc: '完成简单数独' },
];

export const ACHIEVEMENTS = ALL;

export const STORAGE_KEY = 'achievements';

export interface AchievementsState {
  unlocked: string[];
  toast: string | null;
  unlock: (id: string) => void;
  dismissToast: () => void;
}

export const useAchievements = create<AchievementsState>((set) => ({
  unlocked: safeGet<string[]>(STORAGE_KEY, []),
  toast: null,
  unlock: (id) =>
    set((s) => {
      if (s.unlocked.includes(id)) return s;
      const next = [...s.unlocked, id];
      safeSet(STORAGE_KEY, next);
      const ach = ALL.find((a) => a.id === id);
      return {
        unlocked: next,
        toast: ach ? `${ach.name} - ${ach.desc}` : id,
      };
    }),
  dismissToast: () => set({ toast: null }),
}));
