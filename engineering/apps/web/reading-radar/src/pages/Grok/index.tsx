// src/pages/Grok/index.tsx
//
// MVP-6.1 Grok — knowledge-point cards for the `grok` stack.
//
// Data: items-registry.js entries with `stack: "grok"`. Same loader as
// Kanban/Learn (`@/data/tech.ts`). Each card links to the Learn view and
// the Quiz view for the same item.

import { useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import { Card } from '@shared/ui/Card';
import { loadTechItems } from '@/data/tech';
import type { TechItem } from '@/data/types';

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

export function Grok() {
  const [items, setItems] = useState<TechItem[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    setLoading(true);
    loadTechItems('grok')
      .then(setItems)
      // eslint-disable-next-line no-console
      .catch((err) => console.error('[Grok] loadTechItems failed:', err))
      .finally(() => setLoading(false));
  }, []);

  return (
    <div className="max-w-5xl mx-auto space-y-4">
      <div>
        <h1 className="text-2xl font-bold mb-1">🧠 Grokking 题库</h1>
        <p className="text-sm text-gray-500 dark:text-gray-400">
          底层原理深挖 · {items.length} 个知识点
        </p>
      </div>

      {loading ? (
        <Card className="p-6 text-center text-gray-500 dark:text-gray-400">
          加载中…
        </Card>
      ) : items.length === 0 ? (
        <Card className="p-6 text-center text-gray-500 dark:text-gray-400">
          暂无 Grok 题目
        </Card>
      ) : (
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-3">
          {items.map((it) => (
            <Card key={it.id} className="p-4 flex flex-col">
              <div className="flex items-start justify-between gap-2 mb-1">
                <h3 className="font-medium text-sm text-gray-900 dark:text-gray-100 leading-snug">
                  {it.title}
                </h3>
                <span
                  className={`shrink-0 text-xs px-1.5 py-0.5 rounded ${RING_COLORS[it.ring]}`}
                >
                  {RING_LABEL[it.ring]}
                </span>
              </div>
              <p className="text-xs text-gray-500 dark:text-gray-400 line-clamp-2 mb-2">
                {it.desc}
              </p>
              <div className="text-xs text-gray-400 dark:text-gray-500 mb-3">
                {QUADRANT_LABEL[it.quadrant]}
              </div>
              <div className="flex gap-3 mt-auto pt-2 border-t border-gray-100 dark:border-gray-700 text-xs">
                <Link
                  to={`/learn/grok/${it.id}`}
                  className="text-primary-600 hover:underline dark:text-primary-400"
                >
                  📚 学习
                </Link>
              </div>
            </Card>
          ))}
        </div>
      )}
    </div>
  );
}
