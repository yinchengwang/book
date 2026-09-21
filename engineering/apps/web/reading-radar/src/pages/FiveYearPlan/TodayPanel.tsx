// src/pages/FiveYearPlan/TodayPanel.tsx
//
// 今日快捷打卡：核心工程 + 年度专项 chip 一键切换（移植 legacy renderTodayPanel/quickToggle）。
import { Button } from '@shared/ui/Button';
import {
  CORE_PROJECTS,
  YEAR_EXTRA_ITEMS,
  getDayData,
  type PlanState,
} from './planData';

interface TodayPanelProps {
  state: PlanState;
  today: { year: number; month: number; day: number };
  onToggle: (itemId: string) => void;
  onOpenEditor: () => void;
}

export function TodayPanel({ state, today, onToggle, onOpenEditor }: TodayPanelProps) {
  const data = getDayData(state, today.year, today.month, today.day);
  const extras = YEAR_EXTRA_ITEMS[today.year] ?? [];
  const allItems = [...CORE_PROJECTS.flatMap((p) => p.items), ...extras];

  return (
    <div>
      <h3 className="font-semibold text-gray-900 dark:text-gray-100 mb-3">
        📅 今日打卡 · {today.year}年{today.month}月{today.day}日
      </h3>
      <div className="flex flex-wrap gap-2 mb-3">
        {allItems.map((item) => {
          const isDone = !!data.checks[item.id];
          return (
            <button
              key={item.id}
              type="button"
              title={item.desc}
              onClick={() => onToggle(item.id)}
              className={`px-2.5 py-1 rounded-full text-xs border transition-colors ${
                isDone
                  ? 'bg-emerald-50 border-emerald-300 text-emerald-700 dark:bg-emerald-900/30 dark:border-emerald-700 dark:text-emerald-300'
                  : 'bg-gray-50 border-gray-200 text-gray-600 hover:border-gray-300 dark:bg-gray-800 dark:border-gray-700 dark:text-gray-300'
              }`}
            >
              {isDone ? '✅' : '⬜'} {item.label}
            </button>
          );
        })}
      </div>
      <Button variant="ghost" size="sm" onClick={onOpenEditor}>
        📝 详细编辑
      </Button>
    </div>
  );
}
