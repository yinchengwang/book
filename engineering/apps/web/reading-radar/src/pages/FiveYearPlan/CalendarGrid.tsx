// src/pages/FiveYearPlan/CalendarGrid.tsx
//
// 月历网格：月份导航 + 每日完成度圆点 + 笔记标记（移植 legacy renderCalendar）。
import {
  CORE_PROJECTS,
  YEAR_EXTRA_ITEMS,
  countDayChecks,
  dateKey,
  getDayData,
  groupStatus,
  monthGrid,
  type PlanState,
} from './planData';

interface CalendarGridProps {
  year: number;
  month: number; // 1-12
  state: PlanState;
  onPrevMonth: () => void;
  onNextMonth: () => void;
  onSelectDay: (key: string) => void;
}

const WEEKDAYS = ['日', '一', '二', '三', '四', '五', '六'] as const;

const DOT_CLASS: Record<'done' | 'partial' | 'miss', string> = {
  done: 'bg-emerald-500',
  partial: 'bg-amber-400',
  miss: 'bg-gray-300 dark:bg-gray-600',
};

export function CalendarGrid({
  year,
  month,
  state,
  onPrevMonth,
  onNextMonth,
  onSelectDay,
}: CalendarGridProps) {
  const today = new Date();
  const isCurrentMonth = today.getFullYear() === year && today.getMonth() + 1 === month;
  const { firstWeekday, daysInMonth } = monthGrid(year, month);
  const extras = YEAR_EXTRA_ITEMS[year] ?? [];

  const cells: (number | null)[] = [
    ...Array.from({ length: firstWeekday }, () => null),
    ...Array.from({ length: daysInMonth }, (_, i) => i + 1),
  ];

  return (
    <div>
      <div className="flex items-center justify-between mb-3">
        <button
          type="button"
          onClick={onPrevMonth}
          aria-label="上一月"
          className="px-2 py-1 rounded hover:bg-gray-100 dark:hover:bg-gray-800 text-gray-600 dark:text-gray-300"
        >
          ←
        </button>
        <h3 className="font-semibold text-gray-900 dark:text-gray-100">
          {year}年 {month}月
        </h3>
        <button
          type="button"
          onClick={onNextMonth}
          aria-label="下一月"
          className="px-2 py-1 rounded hover:bg-gray-100 dark:hover:bg-gray-800 text-gray-600 dark:text-gray-300"
        >
          →
        </button>
      </div>

      <div className="grid grid-cols-7 gap-1 text-center text-xs text-gray-400 mb-1">
        {WEEKDAYS.map((w) => (
          <div key={w}>{w}</div>
        ))}
      </div>

      <div className="grid grid-cols-7 gap-1">
        {cells.map((day, i) => {
          if (day === null) return <div key={`e${i}`} />;
          const data = getDayData(state, year, month, day);
          const { done, total } = countDayChecks(data, year);
          const isToday = isCurrentMonth && day === today.getDate();
          const hasNote = !!data.note?.trim();
          const hasRecord = done > 0 || hasNote;
          return (
            <button
              key={day}
              type="button"
              data-testid={`day-${dateKey(year, month, day)}`}
              onClick={() => onSelectDay(dateKey(year, month, day))}
              className={`relative aspect-square rounded-md border text-sm flex flex-col items-center justify-center gap-0.5 transition-colors
                ${isToday
                  ? 'border-primary-500 ring-1 ring-primary-500 font-bold'
                  : 'border-gray-200 dark:border-gray-700 hover:border-primary-300 dark:hover:border-primary-700'}
                ${hasRecord ? 'bg-white dark:bg-gray-800' : 'bg-gray-50 dark:bg-gray-900/50 text-gray-400'}`}
            >
              <span>{day}</span>
              <span className="flex gap-0.5" aria-label={`完成 ${done}/${total}`}>
                {CORE_PROJECTS.map((proj) => (
                  <span
                    key={proj.id}
                    className={`w-1.5 h-1.5 rounded-full ${DOT_CLASS[groupStatus(data, proj)]}`}
                  />
                ))}
                {extras.length > 0 && (
                  <span
                    className={`w-1.5 h-1.5 rounded-full ${
                      extras.every((it) => data.checks[it.id])
                        ? DOT_CLASS.done
                        : extras.some((it) => data.checks[it.id])
                          ? DOT_CLASS.partial
                          : DOT_CLASS.miss
                    }`}
                  />
                )}
              </span>
              {hasNote && (
                <span className="absolute top-0.5 right-0.5 text-[9px]" aria-label="有笔记">
                  📝
                </span>
              )}
            </button>
          );
        })}
      </div>

      <div className="flex gap-4 mt-3 text-xs text-gray-500 dark:text-gray-400">
        <span className="flex items-center gap-1">
          <span className={`w-2 h-2 rounded-full ${DOT_CLASS.done}`} /> 全部完成
        </span>
        <span className="flex items-center gap-1">
          <span className={`w-2 h-2 rounded-full ${DOT_CLASS.partial}`} /> 部分完成
        </span>
        <span className="flex items-center gap-1">
          <span className={`w-2 h-2 rounded-full ${DOT_CLASS.miss}`} /> 未打卡
        </span>
      </div>
    </div>
  );
}
