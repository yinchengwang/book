// src/pages/Home/index.tsx
//
// MVP-6.1 Home — knowledge map entry with SVG radar overlay.
//
// MVP-5.1: 8 stack cards (kept).
// MVP-6.1 addition: SVG radar chart, one vertex per stack, radius =
// mastery rate computed from `kanban-progress` × `loadAllTechItems()`.
// Pure SVG, no chart library.

import { useEffect, useMemo, useState } from 'react';
import { Link } from 'react-router-dom';
import { loadAllTechItems } from '@/data/tech';
import { safeGet } from '@shared/storage/safeStorage';
import type { TechCategory, TechItem } from '@/data/types';

type Progress = Record<string, Record<string, string | undefined>>;

const PROGRESS_KEY = 'kanban-progress';

const stacks = [
  { key: 'c', title: 'C 语言', desc: '指针 / 内存 / 编译' },
  { key: 'cpp', title: 'C++', desc: '面向对象 / STL / 模板' },
  { key: 'ds', title: '数据结构', desc: '数组 / 树 / 图' },
  { key: 'db', title: '数据库', desc: 'SQL / 事务 / 索引' },
  { key: 'py', title: 'Python', desc: '语法 / 标准库 / 生态' },
  { key: 'linux', title: 'Linux', desc: 'Shell / 进程 / 网络' },
  { key: 'vdb', title: '向量库', desc: 'Milvus / pgvector' },
  { key: 'grok', title: 'Grokking', desc: '底层原理深挖' },
] as const;

type Cat = (typeof stacks)[number]['key'];

const CATEGORY_ORDER: Cat[] = ['c', 'cpp', 'ds', 'db', 'py', 'linux', 'vdb', 'grok'];

interface VertexStat {
  key: Cat;
  total: number;
  mastered: number;
  rate: number; // 0..1
}

/** Regular polygon vertex points (uniform radius). */
function regularPolygonPoints(
  cx: number,
  cy: number,
  r: number,
  n: number,
  startAngle = -Math.PI / 2
): string {
  const pts: string[] = [];
  for (let i = 0; i < n; i++) {
    const angle = startAngle + (i * 2 * Math.PI) / n;
    const x = cx + r * Math.cos(angle);
    const y = cy + r * Math.sin(angle);
    pts.push(`${x.toFixed(1)},${y.toFixed(1)}`);
  }
  return pts.join(' ');
}

/** Polygon vertex points using a per-vertex radius (data polygon). */
function dataPolygonPoints(
  cx: number,
  cy: number,
  radii: number[],
  startAngle = -Math.PI / 2
): string {
  const pts: string[] = [];
  for (let i = 0; i < radii.length; i++) {
    const angle = startAngle + (i * 2 * Math.PI) / radii.length;
    const x = cx + radii[i] * Math.cos(angle);
    const y = cy + radii[i] * Math.sin(angle);
    pts.push(`${x.toFixed(1)},${y.toFixed(1)}`);
  }
  return pts.join(' ');
}

/** Position at vertex i on the radar. */
function vertexPos(
  cx: number,
  cy: number,
  r: number,
  i: number,
  n: number,
  startAngle = -Math.PI / 2
): { x: number; y: number } {
  const angle = startAngle + (i * 2 * Math.PI) / n;
  return { x: cx + r * Math.cos(angle), y: cy + r * Math.sin(angle) };
}

export function Home() {
  const [stats, setStats] = useState<VertexStat[]>([]);

  useEffect(() => {
    loadAllTechItems()
      .then((all) => {
        const progress = safeGet<Progress>(PROGRESS_KEY, {});
        const out: VertexStat[] = CATEGORY_ORDER.map((k) => {
          const items: TechItem[] = all[k as TechCategory] ?? [];
          const total = items.length;
          let mastered = 0;
          for (const it of items) {
            if (progress[k]?.[it.id] === 'mastered') mastered++;
          }
          return {
            key: k,
            total,
            mastered,
            rate: total > 0 ? mastered / total : 0,
          };
        });
        setStats(out);
      })
      // eslint-disable-next-line no-console
      .catch((err) => console.error('[Home] loadAllTechItems failed:', err));
  }, []);

  // Radar geometry
  const cx = 160;
  const cy = 160;
  const R = 110;
  const n = CATEGORY_ORDER.length;

  const stackMeta = useMemo(() => {
    const m: Record<string, { title: string }> = {};
    for (const s of stacks) m[s.key] = { title: s.title };
    return m;
  }, []);

  const dataPoints = stats.length
    ? dataPolygonPoints(cx, cy, stats.map((s) => R * s.rate))
    : '';

  return (
    <div className="max-w-6xl mx-auto space-y-6">
      <div>
        <h1 className="text-3xl font-bold mb-2">📚 知识地图</h1>
        <p className="text-gray-600 dark:text-gray-400">
          雷达图展示掌握进度，下方卡片按技术栈进入。
        </p>
      </div>

      {/* Radar chart */}
      <section className="bg-white dark:bg-gray-800 rounded-lg shadow-sm border border-gray-200 dark:border-gray-700 p-4">
        <div className="flex items-center justify-between mb-2">
          <h2 className="text-lg font-semibold">🎯 掌握率雷达</h2>
          <span className="text-xs text-gray-500 dark:text-gray-400">
            基于看板进度 (kanban-progress)
          </span>
        </div>

        <div className="overflow-x-auto">
          <svg
            viewBox="0 0 320 320"
            className="w-full max-w-sm mx-auto"
            role="img"
            aria-label="Stack mastery radar"
          >
            {/* Concentric grid rings (25/50/75/100%) */}
            {[0.25, 0.5, 0.75, 1].map((p) => (
              <polygon
                key={p}
                points={regularPolygonPoints(cx, cy, R * p, n)}
                fill="none"
                stroke="currentColor"
                className="text-gray-300 dark:text-gray-600"
                strokeWidth={p === 1 ? 1.5 : 0.75}
                strokeDasharray={p === 1 ? undefined : '3 3'}
              />
            ))}

            {/* Axis lines */}
            {CATEGORY_ORDER.map((_, i) => {
              const p = vertexPos(cx, cy, R, i, n);
              return (
                <line
                  key={i}
                  x1={cx}
                  y1={cy}
                  x2={p.x}
                  y2={p.y}
                  stroke="currentColor"
                  className="text-gray-300 dark:text-gray-600"
                  strokeWidth={0.75}
                />
              );
            })}

            {/* Data polygon */}
            {dataPoints && (
              <polygon
                points={dataPoints}
                fill="rgba(99, 102, 241, 0.25)"
                stroke="rgb(99, 102, 241)"
                strokeWidth={2}
              />
            )}

            {/* Vertex dots */}
            {stats.map((s, i) => {
              const p = vertexPos(cx, cy, R * s.rate, i, n);
              return (
                <circle
                  key={s.key}
                  cx={p.x}
                  cy={p.y}
                  r={3}
                  fill="rgb(99, 102, 241)"
                />
              );
            })}

            {/* Outer labels */}
            {CATEGORY_ORDER.map((k, i) => {
              const p = vertexPos(cx, cy, R + 18, i, n);
              return (
                <text
                  key={k}
                  x={p.x}
                  y={p.y}
                  textAnchor="middle"
                  dominantBaseline="middle"
                  className="fill-gray-700 dark:fill-gray-300"
                  fontSize={10}
                  fontWeight={500}
                >
                  {stackMeta[k]?.title ?? k}
                </text>
              );
            })}
          </svg>
        </div>

        {/* Stats table under the radar */}
        <div className="grid grid-cols-2 sm:grid-cols-4 gap-2 mt-3">
          {stats.map((s) => (
            <div
              key={s.key}
              className="text-center px-2 py-1 rounded bg-gray-50 dark:bg-gray-800/50"
            >
              <div className="text-xs text-gray-500 dark:text-gray-400">
                {stackMeta[s.key]?.title ?? s.key}
              </div>
              <div className="text-sm font-semibold text-gray-800 dark:text-gray-100">
                {s.mastered}/{s.total}
                <span className="ml-1 text-xs text-primary-600 dark:text-primary-400">
                  {Math.round(s.rate * 100)}%
                </span>
              </div>
            </div>
          ))}
        </div>
      </section>

      {/* Stack cards (preserved from MVP-5.1) */}
      <section>
        <h2 className="text-lg font-semibold mb-3">📂 进入学习</h2>
        <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-4 gap-4">
          {stacks.map((s) => (
            <Link
              key={s.key}
              to={`/learn/${s.key}`}
              className="p-4 bg-white dark:bg-gray-800 rounded-lg shadow hover:shadow-lg transition-shadow block border border-gray-100 dark:border-gray-700"
            >
              <h3 className="text-lg font-semibold">{s.title}</h3>
              <p className="text-sm text-gray-500 dark:text-gray-400 mt-1">
                {s.desc}
              </p>
            </Link>
          ))}
        </div>
      </section>
    </div>
  );
}
