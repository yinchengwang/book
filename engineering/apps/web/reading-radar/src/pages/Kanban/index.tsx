// src/pages/Kanban/index.tsx
//
// MVP-5.4 Kanban — learning progress board.
//
// Three-column layout (未开始 / 学习中 / 已掌握).
// Category tab bar at top. Click a card to cycle its status.
// Progress persisted to localStorage key `kanban-progress`.

import { useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import { Card } from '@shared/ui/Card';
import { safeGet, safeSet } from '@shared/storage/safeStorage';
import { loadTechItems } from '@/data/tech';
import type { TechCategory, TechItem } from '@/data/types';

type Status = 'learning' | 'mastered';
type Progress = Record<string, Record<string, Status | undefined>>;

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

const RING_COLORS: Record<TechItem['ring'], string> = {
  basic: 'bg-emerald-100 text-emerald-700 dark:bg-emerald-900/40 dark:text-emerald-300',
  intermediate:
    'bg-amber-100 text-amber-700 dark:bg-amber-900/40 dark:text-amber-300',
  advanced: 'bg-rose-100 text-rose-700 dark:bg-rose-900/40 dark:text-rose-300',
};

const RING_LABEL: Record<TechItem['ring'], string> = {
  basic: '基础',
  intermediate: '进阶',
  advanced: '高级',
};

const QUADRANT_LABEL: Record<TechItem['quadrant'], string> = {
  language: '语言核心',
  systems: '系统编程',
  algorithms: '算法',
  engineering: '工程实践',
};

/** Cycle the status of one item: undefined → learning → mastered → undefined. */
function cycleItem(
  progress: Progress,
  cat: TechCategory,
  itemId: string
): Progress {
  const cur = progress[cat]?.[itemId];
  const next: Status | undefined =
    cur === undefined
      ? 'learning'
      : cur === 'learning'
        ? 'mastered'
        : undefined;

  const patched = { ...progress, [cat]: { ...progress[cat] } };
  if (next === undefined) {
    delete patched[cat]![itemId];
  } else {
    patched[cat]![itemId] = next;
  }
  return patched;
}

interface ColumnProps {
  title: string;
  emoji: string;
  status: Status | undefined;
  items: TechItem[];
  cat: TechCategory;
  onCycle: (itemId: string) => void;
  headerClassName?: string;
}

function Column({
  title,
  emoji,
  status,
  items,
  cat,
  onCycle,
  headerClassName,
}: ColumnProps) {
  return (
    <div className="flex flex-col gap-3">
      <div
        className={`rounded-t-lg px-3 py-2 text-sm font-semibold flex items-center gap-2 ${headerClassName}`}
      >
        <span>{emoji}</span>
        <span>
          {title}
          <span className="ml-2 text-xs font-normal opacity-70">{items.length}</span>
        </span>
      </div>
      <div className="flex flex-col gap-2 pb-4">
        {items.length === 0 && (
          <p className="text-xs text-gray-400 text-center py-6">—</p>
        )}
        {items.map((item) => (
          <Card
            key={item.id}
            className="p-3 cursor-pointer hover:shadow-md transition-shadow group"
          >
            <button
              type="button"
              onClick={() => onCycle(item.id)}
              className="w-full text-left"
              title={`点击切换状态${
                status === 'learning'
                  ? ' → 已掌握'
                  : status === 'mastered'
                    ? ' → 未开始'
                    : ' → 学习中'
              }`}
            >
              <div className="flex items-start justify-between gap-2">
                <span className="font-medium text-sm text-gray-800 dark:text-gray-100 leading-tight">
                  {item.title}
                </span>
                <span
                  className={`text-xs px-1.5 py-0.5 rounded shrink-0 ${RING_COLORS[item.ring]}`}
                >
                  {RING_LABEL[item.ring]}
                </span>
              </div>
              <div className="flex items-center gap-2 mt-1.5 text-xs text-gray-500 dark:text-gray-400">
                <span className="opacity-70">
                  {QUADRANT_LABEL[item.quadrant]}
                </span>
                {status && (
                  <>
                    <span className="opacity-30">·</span>
                    <span
                      className={
                        status === 'learning'
                          ? 'text-blue-600 dark:text-blue-400'
                          : 'text-emerald-600 dark:text-emerald-400'
                      }
                    >
                      {status === 'learning' ? '学习中' : '已掌握'}
                    </span>
                  </>
                )}
              </div>
            </button>
            <div className="flex gap-2 mt-2 pt-2 border-t border-gray-100 dark:border-gray-700 text-xs opacity-0 group-hover:opacity-100 transition-opacity">
              <Link
                to={`/learn/${cat}/${item.id}`}
                className="text-gray-500 hover:text-primary-500 dark:text-gray-400 dark:hover:text-primary-400"
              >
                📚 学习
              </Link>
              <Link
                to={`/quiz/${cat}/${item.id}`}
                className="text-gray-500 hover:text-primary-500 dark:text-gray-400 dark:hover:text-primary-400"
              >
                🎯 测验
              </Link>
            </div>
          </Card>
        ))}
      </div>
    </div>
  );
}

export function Kanban() {
  const [cat, setCat] = useState<TechCategory>('c');
  const [items, setItems] = useState<TechItem[]>([]);
  const [loading, setLoading] = useState(true);
  const [progress, setProgress] = useState<Progress>(() =>
    safeGet(PROGRESS_KEY, {})
  );

  useEffect(() => {
    setLoading(true);
    loadTechItems(cat)
      .then(setItems)
      .catch((err: unknown) => {
        // eslint-disable-next-line no-console
        console.error('[Kanban] loadTechItems failed:', err);
      })
      .finally(() => setLoading(false));
  }, [cat]);

  const handleCycle = (itemId: string) => {
    const next = cycleItem(progress, cat, itemId);
    setProgress(next);
    safeSet(PROGRESS_KEY, next);
  };

  const notStarted = items.filter((i) => progress[cat]?.[i.id] === undefined);
  const learning = items.filter((i) => progress[cat]?.[i.id] === 'learning');
  const mastered = items.filter((i) => progress[cat]?.[i.id] === 'mastered');

  return (
    <div className="max-w-6xl mx-auto space-y-4">
      <div className="flex items-center justify-between">
        <h1 className="text-2xl font-bold">📋 学习看板</h1>
        <span className="text-sm text-gray-500 dark:text-gray-400">
          点击卡片循环切换状态
        </span>
      </div>

      {/* Category tab bar */}
      <div className="flex gap-2 flex-wrap">
        {CATEGORIES.map((c) => (
          <button
            key={c}
            onClick={() => setCat(c)}
            className={`px-3 py-1.5 rounded-md text-sm font-medium transition-colors ${
              cat === c
                ? 'bg-primary-500 text-white'
                : 'bg-gray-100 text-gray-700 hover:bg-gray-200 dark:bg-gray-800 dark:text-gray-300 dark:hover:bg-gray-700'
            }`}
          >
            {CATEGORY_LABEL[c]}
          </button>
        ))}
      </div>

      {/* Summary */}
      <div className="flex gap-4 text-sm text-gray-500 dark:text-gray-400">
        <span>
          总计 <strong className="text-gray-700 dark:text-gray-200">
            {items.length}
          </strong>{' '}
          个知识点
        </span>
        <span>
          已掌握{' '}
          <strong className="text-emerald-600 dark:text-emerald-400">
            {mastered.length}
          </strong>
        </span>
        <span>
          学习中{' '}
          <strong className="text-blue-600 dark:text-blue-400">
            {learning.length}
          </strong>
        </span>
        <span>
          未开始{' '}
          <strong className="text-gray-500 dark:text-gray-400">
            {notStarted.length}
          </strong>
        </span>
      </div>

      {/* 3-column board */}
      {loading ? (
        <p className="text-gray-500 dark:text-gray-400 py-8 text-center">
          加载中…
        </p>
      ) : (
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
          <Column
            title="未开始"
            emoji="⬜"
            status={undefined}
            items={notStarted}
            cat={cat}
            onCycle={handleCycle}
            headerClassName="bg-gray-100 text-gray-600 dark:bg-gray-800 dark:text-gray-300"
          />
          <Column
            title="学习中"
            emoji="📘"
            status="learning"
            items={learning}
            cat={cat}
            onCycle={handleCycle}
            headerClassName="bg-blue-50 text-blue-700 dark:bg-blue-900/30 dark:text-blue-300"
          />
          <Column
            title="已掌握"
            emoji="✅"
            status="mastered"
            items={mastered}
            cat={cat}
            onCycle={handleCycle}
            headerClassName="bg-emerald-50 text-emerald-700 dark:bg-emerald-900/30 dark:text-emerald-300"
          />
        </div>
      )}

      <div className="text-xs text-gray-400 dark:text-gray-500">
        进度保存在浏览器 localStorage，清除缓存会重置。
      </div>
    </div>
  );
}
