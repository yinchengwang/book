// src/games/g2048/animation.ts
export const easing = {
  slideIn: 'cubic-bezier(0.25, 0.1, 0.25, 1)',
  popIn: 'cubic-bezier(0.18, 0.89, 0.32, 1.28)',
};

export const animationDurations = {
  slide: 200,
  pop: 200,
  merge: 150,
} as const;

// 缓动函数（用于 canvas 帧动画）
export function easeInOut(t: number): number {
  return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
}

export function easeOutBack(t: number): number {
  const c1 = 1.70158;
  const c3 = c1 + 1;
  return 1 + c3 * Math.pow(t - 1, 3) + c1 * Math.pow(t - 1, 2);
}