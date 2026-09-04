// src/games/g2048/gesture.ts
import { useRef } from 'react';

export type Direction = 0 | 1 | 2 | 3; // 0=up, 1=down, 2=left, 3=right

export interface Point {
  x: number;
  y: number;
}

/**
 * Detect swipe direction from start to end point.
 * Returns null if the gesture is below the threshold (treated as a tap).
 */
export function detectSwipe(
  start: Point,
  end: Point,
  threshold = 30
): Direction | null {
  const dx = end.x - start.x;
  const dy = end.y - start.y;
  if (Math.abs(dx) < threshold && Math.abs(dy) < threshold) return null;
  if (Math.abs(dx) > Math.abs(dy)) return dx > 0 ? 3 : 2;
  return dy > 0 ? 1 : 0;
}

/**
 * Hook that returns touch start/end handlers that fire onSwipe(direction) when
 * a valid swipe is detected. Designed to spread onto a wrapping element.
 */
export function useSwipeHandlers(onSwipe: (dir: Direction) => void) {
  const startRef = useRef<Point | null>(null);

  return {
    onTouchStart: (e: React.TouchEvent) => {
      const t = e.touches[0];
      if (t) startRef.current = { x: t.clientX, y: t.clientY };
    },
    onTouchEnd: (e: React.TouchEvent) => {
      const start = startRef.current;
      startRef.current = null;
      if (!start) return;
      const t = e.changedTouches[0];
      if (!t) return;
      const dir = detectSwipe(start, { x: t.clientX, y: t.clientY });
      if (dir !== null) onSwipe(dir);
    },
  };
}
