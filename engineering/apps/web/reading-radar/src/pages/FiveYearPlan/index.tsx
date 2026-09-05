// src/pages/FiveYearPlan/index.tsx
//
// MVP-6.1 Five-Year Plan — yearly themes + today's daily-check tracker.
//
// Data sources:
//   - YEAR_CONFIG: hardcoded below (sourced from the legacy
//     five-year-plan.html — the spec didn't expose this in a JSON).
//     Each year has a name, core focus, and 3-4 actionable themes.
//   - Daily checks: `user-data/state/five-year-plan-state.json`
//     (read-only here — full editor lives in legacy page). Shown for
//     the most recent date that has entries.
//
// Persisted state: localStorage key `five-year-plan-checks-mvp` for the
// MVP-grade checkbox UI. Real per-day state is preserved on disk by the
// legacy page; the MVP card just lets the user browse the schedule.

import { useMemo, useState } from 'react';
import { Card } from '@shared/ui/Card';
import { safeGet, safeSet } from '@shared/storage/safeStorage';
import stateSource from '../../../user-data/state/five-year-plan-state.json?raw';

interface YearConfig {
  year: number;
  name: string;
  theme: string;
  emoji: string;
  actions: string[];
}

interface CheckEntry {
  done: boolean;
  label: string;
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

const CORE_CHECKS: CheckEntry[] = [
  { done: false, label: '🏃 运动 30min+' },
  { done: false, label: '📖 阅读 30min+' },
  { done: false, label: '💻 深度工作 2h+' },
  { done: false, label: '📓 每日复盘' },
];

const STORAGE_KEY = 'five-year-plan-checks-mvp';

function parseState(raw: string): {
  mostRecent?: { date: string; checks: number; total: number; note?: string };
} {
  try {
    const data = JSON.parse(raw) as Record<
      string,
      Record<string, Record<string, { checks: Record<string, boolean>; note?: string }>>
    >;
    // Flatten and find the most recent date with entries.
    let best: { date: string; checks: number; total: number; note?: string } | undefined;
    for (const [year, months] of Object.entries(data)) {
      for (const [month, days] of Object.entries(months)) {
        for (const [day, entry] of Object.entries(days)) {
          const checks = entry.checks ?? {};
          const total = Object.keys(checks).length;
          if (total === 0) continue;
          const done = Object.values(checks).filter(Boolean).length;
          const date = `${year}-${month}-${day}`;
          if (!best || date > best.date) {
            best = { date, checks: done, total, note: entry.note };
          }
        }
      }
    }
    return { mostRecent: best };
  } catch {
    return {};
  }
}

const parsed = parseState(stateSource);

export function FiveYearPlan() {
  const [activeYear, setActiveYear] = useState<number>(YEAR_CONFIG[0]?.year ?? 2026);
  const [checks, setChecks] = useState<CheckEntry[]>(() =>
    safeGet(STORAGE_KEY, CORE_CHECKS)
  );

  const active = useMemo(
    () => YEAR_CONFIG.find((y) => y.year === activeYear) ?? YEAR_CONFIG[0],
    [activeYear]
  );

  const doneCount = checks.filter((c) => c.done).length;
  const pct = Math.round((doneCount / checks.length) * 100);

  const toggle = (idx: number) => {
    const next = checks.map((c, i) => (i === idx ? { ...c, done: !c.done } : c));
    setChecks(next);
    safeSet(STORAGE_KEY, next);
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

      {/* Today's checks (MVP-grade) */}
      <Card className="p-5">
        <div className="flex items-center justify-between mb-3">
          <h3 className="font-semibold text-gray-900 dark:text-gray-100">
            ✅ 今日核心打卡（MVP）
          </h3>
          <span className="text-xs text-gray-500 dark:text-gray-400">
            {doneCount}/{checks.length} ({pct}%)
          </span>
        </div>
        <div className="w-full h-2 bg-gray-100 dark:bg-gray-800 rounded mb-4 overflow-hidden">
          <div
            className="h-full bg-emerald-500 transition-all"
            style={{ width: `${pct}%` }}
          />
        </div>
        <div className="space-y-2">
          {checks.map((c, i) => (
            <label
              key={i}
              className="flex items-center gap-3 cursor-pointer hover:bg-gray-50 dark:hover:bg-gray-800/60 p-2 rounded"
            >
              <input
                type="checkbox"
                checked={c.done}
                onChange={() => toggle(i)}
                className="w-4 h-4 accent-emerald-500"
              />
              <span
                className={`text-sm ${c.done ? 'line-through text-gray-400' : 'text-gray-700 dark:text-gray-200'}`}
              >
                {c.label}
              </span>
            </label>
          ))}
        </div>
        <p className="text-xs text-gray-400 dark:text-gray-500 mt-3">
          保存到 localStorage · {STORAGE_KEY}
        </p>
      </Card>

      {/* Most recent legacy snapshot */}
      {parsed.mostRecent && (
        <Card className="p-4">
          <h3 className="font-semibold text-sm text-gray-900 dark:text-gray-100 mb-2">
            📅 历史打卡快照（来自 user-data/state）
          </h3>
          <p className="text-sm text-gray-700 dark:text-gray-300">
            <strong>{parsed.mostRecent.date}</strong> · 今日完成{' '}
            <strong className="text-emerald-600 dark:text-emerald-400">
              {parsed.mostRecent.checks}
            </strong>{' '}
            / {parsed.mostRecent.total} 项
          </p>
          {parsed.mostRecent.note && (
            <p className="text-sm text-gray-500 dark:text-gray-400 mt-2 italic">
              "{parsed.mostRecent.note}"
            </p>
          )}
          <p className="text-xs text-gray-400 dark:text-gray-500 mt-2">
            详细每日打卡请使用原始 five-year-plan.html（带日历视图）
          </p>
        </Card>
      )}
    </div>
  );
}
