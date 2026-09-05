// src/pages/Dashboard/index.tsx
//
// MVP-5.4 Dashboard — progress overview.
//
// Reads `kanban-progress` from localStorage, combines with `loadAllTechItems`
// to compute real statistics. No chart library — Tailwind divs for bar charts.
//
// Displays:
//   1. Overall mastery rate (mastered / total)
//   2. Count of items currently "learning"
//   3. Per-category breakdown with horizontal stacked bars

import { useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import { safeGet } from '@shared/storage/safeStorage';
import { loadAllTechItems } from '@/data/tech';
import type { TechCategory, TechItem } from '@/data/types';

type Progress = Record<string, Record<string, string | undefined>>;

const PROGRESS_KEY = 'kanban-progress';

const CATEGORIES = [
  'c',
  'cpp',
  'ds',
  'db',
  'py',
  'linux',
  'vdb',
  'grok',
] as const satisfies readonly TechCategory[];

const CATEGORY_LABEL: Record<TechCategory, string> = {
  c: 'C',
  cpp: 'C++',
  ds: '数据结构',
  db: '数据库',
  py: 'Python',
  linux: 'Linux',
  vdb: '向量库',
  grok: 'Grokking',
};

interface CatStats {
  cat: TechCategory;
  total: number;
  mastered: number;
  learning: number;
  notStarted: number;
}

function computeStats(
  allItems: Record<TechCategory, TechItem[]>,
  progress: Progress
): CatStats[] {
  return CATEGORIES.map((cat) => {
    const items = allItems[cat] ?? [];
    const catProgress = progress[cat] ?? {};
    let mastered = 0;
    let learning = 0;
    for (const item of items) {
      const s = catProgress[item.id];
      if (s === 'mastered') mastered++;
      else if (s === 'learning') learning++;
    }
    return {
      cat,
      total: items.length,
      mastered,
      learning,
      notStarted: items.length - mastered - learning,
    };
  });
}

function emptyGroups(): Record<TechCategory, TechItem[]> {
  return {
    c: [],
    cpp: [],
    ds: [],
    db: [],
    py: [],
    linux: [],
    vdb: [],
    grok: [],
  };
}

export function Dashboard() {
  const [allItems, setAllItems] = useState<Record<TechCategory, TechItem[]>>(
    emptyGroups
  );
  const [loading, setLoading] = useState(true);
  const [progress] = useState<Progress>(() => safeGet(PROGRESS_KEY, {}));

  useEffect(() => {
    setLoading(true);
    loadAllTechItems()
      .then(setAllItems)
      .catch((err: unknown) => {
        // eslint-disable-next-line no-console
        console.error('[Dashboard] loadAllTechItems failed:', err);
      })
      .finally(() => setLoading(false));
  }, []);

  const catStats = computeStats(allItems, progress);

  const totalItems = catStats.reduce((s, c) => s + c.total, 0);
  const totalMastered = catStats.reduce((s, c) => s + c.mastered, 0);
  const totalLearning = catStats.reduce((s, c) => s + c.learning, 0);
  const totalNotStarted = catStats.reduce((s, c) => s + c.notStarted, 0);
  const masteryRate =
    totalItems > 0 ? Math.round((totalMastered / totalItems) * 100) : 0;

  return (
    <div className="max-w-3xl mx-auto space-y-6">
      <h1 className="text-2xl font-bold">📊 学习仪表盘</h1>

      {/* Summary cards */}
      <div className="grid grid-cols-1 sm:grid-cols-3 gap-4">
        {/* Mastery rate */}
        <div className="bg-white dark:bg-gray-800 rounded-lg shadow-sm border border-gray-200 dark:border-gray-700 p-5">
          <p className="text-sm text-gray-500 dark:text-gray-400 mb-1">
            总掌握率
          </p>
          <p className="text-4xl font-bold text-primary-500">
            {loading ? '…' : `${masteryRate}%`}
          </p>
          <p className="text-xs text-gray-400 mt-2">
            {totalMastered} / {totalItems} 个知识点
          </p>
        </div>

        {/* Learning */}
        <div className="bg-white dark:bg-gray-800 rounded-lg shadow-sm border border-gray-200 dark:border-gray-700 p-5">
          <p className="text-sm text-gray-500 dark:text-gray-400 mb-1">
            学习中
          </p>
          <p className="text-4xl font-bold text-blue-500">
            {loading ? '…' : totalLearning}
          </p>
          <p className="text-xs text-gray-400 mt-2">正在跟进的知识点</p>
        </div>

        {/* Not started */}
        <div className="bg-white dark:bg-gray-800 rounded-lg shadow-sm border border-gray-200 dark:border-gray-700 p-5">
          <p className="text-sm text-gray-500 dark:text-gray-400 mb-1">
            未开始
          </p>
          <p className="text-4xl font-bold text-gray-400 dark:text-gray-500">
            {loading ? '…' : totalNotStarted}
          </p>
          <p className="text-xs text-gray-400 mt-2">待学习知识点</p>
        </div>
      </div>

      {/* Category breakdown */}
      <div className="bg-white dark:bg-gray-800 rounded-lg shadow-sm border border-gray-200 dark:border-gray-700 p-5">
        <h2 className="text-lg font-semibold mb-4">分类明细</h2>
        {loading ? (
          <p className="text-sm text-gray-500 dark:text-gray-400 py-4 text-center">
            加载中…
          </p>
        ) : totalItems === 0 ? (
          <p className="text-sm text-gray-500 dark:text-gray-400 py-4 text-center">
            暂无数据，请先访问看板页添加进度。
          </p>
        ) : (
          <div className="space-y-4">
            {catStats.map((stat) => {
              const rate =
                stat.total > 0
                  ? Math.round((stat.mastered / stat.total) * 100)
                  : 0;
              return (
                <div key={stat.cat}>
                  <div className="flex items-center justify-between text-sm mb-1">
                    <span className="font-medium text-gray-700 dark:text-gray-200">
                      {CATEGORY_LABEL[stat.cat]}
                    </span>
                    <span className="text-xs text-gray-400">
                      {stat.mastered}/{stat.total} · {rate}%
                    </span>
                  </div>
                  {/* Stacked bar: mastered (emerald) + learning (blue) */}
                  <div className="h-2.5 bg-gray-100 dark:bg-gray-700 rounded-full overflow-hidden flex">
                    {stat.mastered > 0 && (
                      <div
                        className="h-full bg-emerald-500"
                        style={{ width: `${(stat.mastered / stat.total) * 100}%` }}
                        title={`已掌握 ${stat.mastered}`}
                      />
                    )}
                    {stat.learning > 0 && (
                      <div
                        className="h-full bg-blue-400"
                        style={{ width: `${(stat.learning / stat.total) * 100}%` }}
                        title={`学习中 ${stat.learning}`}
                      />
                    )}
                  </div>
                  <div className="flex gap-3 mt-1 text-xs text-gray-400">
                    {stat.notStarted > 0 && <span>未开始 {stat.notStarted}</span>}
                    {stat.learning > 0 && <span>学习中 {stat.learning}</span>}
                    {stat.mastered > 0 && (
                      <span className="text-emerald-600 dark:text-emerald-400">
                        已掌握 {stat.mastered}
                      </span>
                    )}
                  </div>
                </div>
              );
            })}
          </div>
        )}
      </div>

      <div className="text-xs text-gray-400 dark:text-gray-500">
        数据来源于学习看板的浏览器本地进度。访问{' '}
        <Link to="/kanban" className="text-primary-500 hover:underline">
          /kanban
        </Link>{' '}
        可更新进度。
      </div>
    </div>
  );
}
