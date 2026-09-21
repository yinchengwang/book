// src/pages/FiveYearPlan/StatsRow.tsx
//
// 四项统计：今日完成率 / 本月完成率 / 连续打卡 / 累计打卡（移植 legacy renderStats）。
import type { PlanStats } from './planData';

export function StatsRow({ stats }: { stats: PlanStats }) {
  const boxes: { num: string | number; label: string; good: boolean }[] = [
    { num: `${stats.todayPct}%`, label: '今日完成率', good: stats.todayPct >= 70 },
    { num: `${stats.monthPct}%`, label: '本月完成率', good: stats.monthPct >= 60 },
    { num: stats.streak, label: '连续打卡 (天)', good: true },
    { num: stats.totalDays, label: '累计打卡 (天)', good: true },
  ];
  return (
    <div className="grid grid-cols-2 sm:grid-cols-4 gap-3">
      {boxes.map((b) => (
        <div
          key={b.label}
          className="rounded-lg border border-gray-200 dark:border-gray-700 bg-white dark:bg-gray-800 p-3 text-center"
        >
          <div
            className={`text-2xl font-bold ${
              b.good
                ? 'text-emerald-600 dark:text-emerald-400'
                : 'text-amber-500 dark:text-amber-400'
            }`}
          >
            {b.num}
          </div>
          <div className="text-xs text-gray-500 dark:text-gray-400 mt-1">{b.label}</div>
        </div>
      ))}
    </div>
  );
}
