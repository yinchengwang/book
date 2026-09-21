// src/pages/FiveYearPlan/index.tsx
//
// Five-Year Plan — yearly themes + 打卡日历系统。
//
// 日历/打卡能力自 five-year-plan-legacy.html 移植（Tech Debt 收口）：
//   - 月历网格（完成度圆点 + 笔记标记 + 月份导航）
//   - 今日快捷打卡 chips 与单日详细编辑弹窗
//   - 今日/本月完成率、连续与累计打卡统计
//   - 数据层与 legacy 页共用 localStorage（five_year_plan_state），
//     可选经 Express /api/state/five-year-plan-state 多设备同步。
//
// 原 MVP 复选卡与只读快照卡已被本系统取代（localStorage key
// `five-year-plan-checks-mvp` 的旧数据不再读取；如需保留请手动迁移）。

import { useMemo, useState } from 'react';
import { Card } from '@shared/ui/Card';
import { CalendarGrid } from './CalendarGrid';
import { DayEditor } from './DayEditor';
import { StatsRow } from './StatsRow';
import { TodayPanel } from './TodayPanel';
import { computeStats, getDayData, parseDateKey } from './planData';
import { usePlanState } from './usePlanState';

interface YearConfig {
  year: number;
  name: string;
  theme: string;
  emoji: string;
  actions: string[];
}

const YEAR_CONFIG: YearConfig[] = [
  {
    year: 2026,
    name: '筑基年',
    theme: '打好基础',
    emoji: '🏗️',
    actions: [
      '养成规律作息、运动、饮食三大习惯',
      '梳理财务（还清高息负债、建立应急金）',
      '确定 1 个长期深耕方向',
      '减少无效社交',
    ],
  },
  {
    year: 2027,
    name: '能力年',
    theme: '提升硬实力',
    emoji: '💪',
    actions: [
      '深耕一项不可替代的核心技能',
      '建立稳定输出（作品、经验、人脉）',
      '学会情绪管理',
      '实现收入稳步提升',
    ],
  },
  {
    year: 2028,
    name: '提质年',
    theme: '优化生活与认知',
    emoji: '✨',
    actions: [
      '改善居住环境与形象',
      '拓展认知（读书、见人、走出去）',
      '建立被动收入/副业体系',
      '修复重要关系',
    ],
  },
  {
    year: 2029,
    name: '抗风险年',
    theme: '建立保障与自由',
    emoji: '🛡️',
    actions: [
      '完善健康、医疗、养老保障',
      '合理配置资产',
      '拥有随时可选择的自由（不被工作绑架）',
      '心态成熟稳定',
    ],
  },
  {
    year: 2030,
    name: '丰收年',
    theme: '全面进入良性循环',
    emoji: '🌾',
    actions: [
      '身体、能力、财富、心态全面向好',
      '拥有清晰的下一个五年方向',
      '成为更可靠、强大、温柔的自己',
    ],
  },
];

export function FiveYearPlan() {
  const now = new Date();
  const today = { year: now.getFullYear(), month: now.getMonth() + 1, day: now.getDate() };

  const [activeYear, setActiveYear] = useState<number>(YEAR_CONFIG[0]?.year ?? 2026);
  const [calYear, setCalYear] = useState(today.year);
  const [calMonth, setCalMonth] = useState(today.month);
  const [editingKey, setEditingKey] = useState<string | null>(null);

  const { state, updateDay, clearDay, toggleCheck } = usePlanState();

  const active = useMemo(
    () => YEAR_CONFIG.find((y) => y.year === activeYear) ?? YEAR_CONFIG[0],
    [activeYear]
  );

  const stats = useMemo(() => computeStats(state, today), [state, today]);

  const prevMonth = () => {
    setCalMonth((m) => {
      if (m > 1) return m - 1;
      setCalYear((y) => y - 1);
      return 12;
    });
  };
  const nextMonth = () => {
    setCalMonth((m) => {
      if (m < 12) return m + 1;
      setCalYear((y) => y + 1);
      return 1;
    });
  };

  return (
    <div className="max-w-5xl mx-auto space-y-4">
      <div>
        <h1 className="text-2xl font-bold mb-1">🏗️ 五年建设计划</h1>
        <p className="text-sm text-gray-500 dark:text-gray-400">
          筑基 → 丰收 · {YEAR_CONFIG[0]?.year} – {YEAR_CONFIG[YEAR_CONFIG.length - 1]?.year}
        </p>
      </div>

      {/* Year tab bar */}
      <div className="flex gap-2 flex-wrap">
        {YEAR_CONFIG.map((y) => (
          <button
            key={y.year}
            type="button"
            onClick={() => setActiveYear(y.year)}
            className={`px-3 py-1.5 rounded-md text-sm font-medium transition-colors ${
              activeYear === y.year
                ? 'bg-primary-500 text-white'
                : 'bg-gray-100 text-gray-700 hover:bg-gray-200 dark:bg-gray-800 dark:text-gray-300 dark:hover:bg-gray-700'
            }`}
          >
            <span className="mr-1">{y.emoji}</span>
            {y.year} {y.name}
          </button>
        ))}
      </div>

      {/* Year theme card */}
      {active && (
        <Card className="p-6 border-l-4 border-primary-500">
          <div className="flex items-start justify-between gap-3 mb-3">
            <div>
              <h2 className="text-xl font-bold text-gray-900 dark:text-gray-100">
                {active.year} · {active.name}
              </h2>
              <p className="text-sm text-primary-600 dark:text-primary-400 mt-1">
                {active.theme}
              </p>
            </div>
            <span className="text-4xl shrink-0">{active.emoji}</span>
          </div>
          <ul className="space-y-1 text-sm text-gray-700 dark:text-gray-300">
            {active.actions.map((a, i) => (
              <li key={i} className="flex gap-2">
                <span className="text-primary-500 shrink-0">▸</span>
                <span>{a}</span>
              </li>
            ))}
          </ul>
        </Card>
      )}

      {/* 打卡统计 */}
      <StatsRow stats={stats} />

      {/* 今日快捷打卡 */}
      <Card className="p-5">
        <TodayPanel
          state={state}
          today={today}
          onToggle={(itemId) => toggleCheck(today.year, today.month, today.day, itemId)}
          onOpenEditor={() =>
            setEditingKey(`${today.year}-${String(today.month).padStart(2, '0')}-${String(today.day).padStart(2, '0')}`)
          }
        />
      </Card>

      {/* 打卡日历 */}
      <Card className="p-5">
        <CalendarGrid
          year={calYear}
          month={calMonth}
          state={state}
          onPrevMonth={prevMonth}
          onNextMonth={nextMonth}
          onSelectDay={setEditingKey}
        />
      </Card>

      {/* 单日编辑弹窗 */}
      {editingKey && (
        <DayEditor
          dateKeyStr={editingKey}
          initial={getDayData(state, ...parseDateKey(editingKey))}
          onSave={updateDay}
          onClear={clearDay}
          onClose={() => setEditingKey(null)}
        />
      )}
    </div>
  );
}
