/**
 * @file components/common/AchievementToast.tsx
 * @brief 解锁成就时的顶部弹窗（队列模式，依次显示多个成就）
 */
import { useEffect, useRef, useState } from "react";
import { View, Text } from "@tarojs/components";
import { subscribe } from "@/services/achievements";
import type { Achievement } from "@/types/achievements";
import "./AchievementToast.scss";

const LABELS: Record<string, string> = {
  snake_first_blood: "初次进食",
  snake_score_100: "贪吃蛇 100 分",
  sudoku_first_clear: "首回数独",
  sudoku_no_hint: "无提示通关",
  game2048_reach_2048: "合出 2048",
  game2048_reach_4096: "合出 4096",
  match3_first_clear: "首次通关消消乐",
  match3_three_stars: "消消乐三星",
};

const SHOW_DURATION = 2400; // 每个 toast 显示时长（ms）

export function AchievementToast() {
  const [current, setCurrent] = useState<Achievement | null>(null);
  const queueRef = useRef<Achievement[]>([]);
  const timerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const isShowingRef = useRef(false);

  // 从队列中取出下一个并显示
  const showNext = () => {
    if (queueRef.current.length === 0) {
      isShowingRef.current = false;
      return;
    }
    isShowingRef.current = true;
    const next = queueRef.current.shift()!;
    setCurrent(next);
    timerRef.current = setTimeout(() => {
      setCurrent(null);
      // 短暂间隔后显示下一个
      timerRef.current = setTimeout(() => showNext(), 300);
    }, SHOW_DURATION);
  };

  useEffect(() => {
    const unsub = subscribe((a) => {
      queueRef.current.push(a);
      // 如果当前没有在显示，立即开始显示队列
      if (!isShowingRef.current) {
        showNext();
      }
    });
    return () => {
      unsub();
      if (timerRef.current) clearTimeout(timerRef.current);
    };
  }, []);

  if (!current) return null;
  return (
    <View className="achievement-toast">
      <Text className="achievement-icon">🏆</Text>
      <View className="achievement-body">
        <Text className="achievement-label">成就解锁</Text>
        <Text className="achievement-name">
          {LABELS[current.id] ?? current.id}
        </Text>
      </View>
    </View>
  );
}

export default AchievementToast;
