// src/pages/Learn/index.tsx
//
// MVP-5.3 Learn page — category listing + knowledge-point deep-dive Markdown.
//
// Routes:
//   /learn             — landing (no topic picked yet)
//   /learn/:cat        — list every TechItem in the category. Home stack
//                        cards link here (single-segment URL); each card
//                        deep-links into the topic view below.
//   /learn/:cat/:item  — topic view; URL provides (cat, itemId), quadrant
//                        is inferred from the tech-items registry.
//
// Data flow (category list):
//   1. Call `loadTechItems(cat)` (Task 4.2 registry loader).
//   2. Group items by quadrant, render one card per item linking to
//      `/learn/:cat/:itemId`.
//
// Data flow (topic view):
//   1. Look the item up in the tech registry to recover its `quadrant`
//      (the route omits it). Fall back to 'language' if not found.
//   2. Call `loadLearnContent(cat, quadrant, itemId)` which returns a
//      LearnContentResult — 'not-found' and 'load-error' render distinct
//      messages instead of fake fallback content (see data/learn.ts).
//   3. Render through the cross-project `@shared/components/Markdown`
//      component (created in Task 5.2; do NOT recreate locally).
//
// Notes:
//   - The shared Markdown component already applies the `prose` classes,
//     so this page only needs to manage loading / error / content layout.
//   - The quiz link uses the matching `/quiz/:cat/:item` route.

import { useEffect, useMemo, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { Markdown } from '@shared/components/Markdown';
import { loadAllTechItems, loadTechItems } from '@/data/tech';
import { loadLearnContent } from '@/data/learn';
import type { Quadrant, TechCategory, TechItem } from '@/data/types';

type Status = 'idle' | 'loading' | 'ready' | 'error';

/**
 * Every category the Learn section serves. Unlike AVAILABLE_CATEGORIES
 * (quiz banks, no `grok`), the learn-deep content exists for all 8 stacks,
 * so Home's 8 stack cards — including Grokking — must resolve here.
 */
const ALL_CATEGORIES = [
  'c',
  'cpp',
  'ds',
  'db',
  'py',
  'linux',
  'vdb',
  'grok',
] as const satisfies readonly TechCategory[];

/** Display name per category, matching the Home stack cards. */
const CATEGORY_TITLES: Record<TechCategory, string> = {
  c: 'C 语言',
  cpp: 'C++',
  ds: '数据结构',
  db: '数据库',
  py: 'Python',
  linux: 'Linux',
  vdb: '向量库',
  grok: 'Grokking',
};

const QUADRANTS: readonly Quadrant[] = [
  'language',
  'systems',
  'algorithms',
  'engineering',
];

const QUADRANT_LABELS: Record<Quadrant, string> = {
  language: '语言核心',
  systems: '系统编程',
  algorithms: '算法',
  engineering: '工程实践',
};

const RING_LABELS: Record<TechItem['ring'], string> = {
  basic: '基础',
  intermediate: '进阶',
  advanced: '高级',
};

/**
 * Default quadrant when the item can't be found in the tech registry.
 * 'language' is the safest default — it's the most common quadrant
 * for C-family basics like 'syntax', 'types', etc.
 */
const DEFAULT_QUADRANT: Quadrant = 'language';

export function Learn() {
  const { cat, item } = useParams();

  // Three mutually exclusive views, keyed off the URL shape.
  // (Topic view = hasCat && item — handled by the fall-through return.)
  const hasCat = Boolean(cat);
  const isCatList = hasCat && !item;

  // Route guard — typed narrow for downstream data calls.
  const typedCat: TechCategory | undefined = useMemo(() => {
    if (!cat) return undefined;
    return (ALL_CATEGORIES as readonly string[]).includes(cat)
      ? (cat as TechCategory)
      : undefined;
  }, [cat]);

  const [status, setStatus] = useState<Status>('idle');
  // Topic view state.
  const [error, setError] = useState<string | null>(null);
  const [errorReason, setErrorReason] = useState<
    'not-found' | 'load-error' | null
  >(null);
  const [content, setContent] = useState<string>('');
  // Category list view state.
  const [items, setItems] = useState<TechItem[]>([]);

  useEffect(() => {
    // Reset all view state whenever the (cat, item) shape changes —
    // handles in-place navigation between views without unmounting.
    setError(null);
    setErrorReason(null);
    setContent('');
    setItems([]);

    if (!cat) {
      setStatus('idle');
      return;
    }

    // Unknown category — an explicit error beats a silently blank page.
    if (!typedCat) {
      setError(`未知分类：${cat}`);
      setStatus('error');
      return;
    }

    let cancelled = false;

    // --- Category list view (/learn/:cat) ------------------------
    if (!item) {
      setStatus('loading');
      (async () => {
        try {
          const list = await loadTechItems(typedCat);
          if (cancelled) return;
          setItems(list);
          setStatus('ready');
        } catch (err: unknown) {
          if (cancelled) return;
          setError(err instanceof Error ? err.message : String(err));
          setStatus('error');
        }
      })();
      return () => {
        cancelled = true;
      };
    }

    // --- Topic view (/learn/:cat/:item) --------------------------
    setStatus('loading');
    (async () => {
      try {
        // Quadrant isn't on the route — recover it from the tech items
        // registry. The registry is keyed by category, so flatten to a
        // single array before searching by id.
        const grouped = await loadAllTechItems();
        const all = Object.values(grouped).flat();
        const meta = all.find((t) => t.id === item);
        const quadrant: Quadrant = meta?.quadrant ?? DEFAULT_QUADRANT;

        const result = await loadLearnContent(typedCat, quadrant, item);
        if (cancelled) return;
        if (result.ok) {
          setContent(result.content);
          setStatus('ready');
        } else {
          setErrorReason(result.reason);
          setStatus('error');
        }
      } catch (err: unknown) {
        if (cancelled) return;
        setError(err instanceof Error ? err.message : String(err));
        setStatus('error');
      }
    })();

    return () => {
      cancelled = true;
    };
  }, [cat, item, typedCat]);

  // --- Landing (/learn) ------------------------------------------
  if (!hasCat) {
    return (
      <div className="max-w-4xl mx-auto space-y-4">
        <h1 className="text-2xl font-bold">📖 学习</h1>
        <p className="text-gray-600 dark:text-gray-400">
          从首页选择一个知识点卡片开始学习。
        </p>
        <Link
          to="/"
          className="inline-block bg-primary-500 text-white px-4 py-2 rounded hover:bg-primary-600"
        >
          ← 返回首页
        </Link>
      </div>
    );
  }

  // --- Category list (/learn/:cat) -------------------------------
  if (isCatList) {
    const byQuadrant = QUADRANTS.map((q) => ({
      quadrant: q,
      items: items.filter((t) => t.quadrant === q),
    })).filter((group) => group.items.length > 0);

    return (
      <div className="max-w-4xl mx-auto space-y-6">
        <div className="text-sm text-gray-500 dark:text-gray-400">
          <Link to="/learn" className="hover:text-primary-500">
            📖 学习
          </Link>
          <span className="mx-1">/</span>
          <code className="px-1 py-0.5 bg-gray-100 dark:bg-gray-800 rounded">
            {cat}
          </code>
        </div>

        <div>
          <h1 className="text-2xl font-bold">
            {typedCat ? CATEGORY_TITLES[typedCat] : cat}
          </h1>
          {status === 'ready' && (
            <p className="text-sm text-gray-500 dark:text-gray-400 mt-1">
              共 {items.length} 个知识点，选择一个开始学习。
            </p>
          )}
        </div>

        {status === 'loading' && (
          <p className="text-gray-500 dark:text-gray-400">加载中…</p>
        )}

        {status === 'error' && (
          <div className="p-4 bg-red-50 dark:bg-red-900/30 border border-red-200 dark:border-red-800 rounded">
            <p className="text-red-700 dark:text-red-300 font-medium">
              加载失败
            </p>
            {error && (
              <p className="text-sm text-red-600 dark:text-red-400 mt-1">
                {error}
              </p>
            )}
          </div>
        )}

        {status === 'ready' && items.length === 0 && (
          <p className="text-gray-500 dark:text-gray-400">
            该分类下暂无知识点。
          </p>
        )}

        {status === 'ready' &&
          byQuadrant.map(({ quadrant, items: group }) => (
            <section key={quadrant}>
              <h2 className="text-lg font-semibold mb-3">
                {QUADRANT_LABELS[quadrant]}
                <span className="text-sm font-normal text-gray-400 ml-2">
                  {group.length}
                </span>
              </h2>
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
                {group.map((t) => (
                  <Link
                    key={t.id}
                    to={`/learn/${cat}/${t.id}`}
                    className="p-4 bg-white dark:bg-gray-800 rounded-lg shadow hover:shadow-lg transition-shadow block border border-gray-100 dark:border-gray-700"
                  >
                    <div className="flex items-center justify-between gap-2">
                      <span className="font-medium">{t.title}</span>
                      <span className="text-xs text-gray-400 dark:text-gray-500 shrink-0">
                        {RING_LABELS[t.ring]}
                      </span>
                    </div>
                    <p className="text-sm text-gray-500 dark:text-gray-400 mt-1 line-clamp-2">
                      {t.desc}
                    </p>
                  </Link>
                ))}
              </div>
            </section>
          ))}

        <div className="flex gap-4 text-sm">
          <Link
            to="/learn"
            className="text-primary-500 hover:text-primary-600"
          >
            ← 返回学习首页
          </Link>
          <Link to="/" className="text-primary-500 hover:text-primary-600">
            返回首页
          </Link>
        </div>
      </div>
    );
  }

  // --- Topic view (/learn/:cat/:item) ----------------------------
  return (
    <div className="max-w-4xl mx-auto">
      <div className="mb-4 text-sm text-gray-500 dark:text-gray-400">
        <Link to="/learn" className="hover:text-primary-500">
          📖 学习
        </Link>
        <span className="mx-1">/</span>
        {typedCat ? (
          <Link to={`/learn/${cat}`} className="hover:text-primary-500">
            {CATEGORY_TITLES[typedCat] ?? cat}
          </Link>
        ) : (
          <code className="px-1 py-0.5 bg-gray-100 dark:bg-gray-800 rounded">
            {cat}
          </code>
        )}
        <span className="mx-1">/</span>
        <code className="px-1 py-0.5 bg-gray-100 dark:bg-gray-800 rounded">
          {item}
        </code>
      </div>

      {status === 'loading' && (
        <p className="text-gray-500 dark:text-gray-400">加载中…</p>
      )}

      {status === 'error' && (
        <div className="p-4 bg-red-50 dark:bg-red-900/30 border border-red-200 dark:border-red-800 rounded">
          <p className="text-red-700 dark:text-red-300 font-medium">
            内容加载失败
          </p>
          <p className="text-sm text-red-600 dark:text-red-400 mt-1">
            {errorReason === 'not-found'
              ? '该知识点不存在或暂无详细内容'
              : errorReason === 'load-error'
                ? '网络或文件错误，请稍后重试'
                : error}
          </p>
        </div>
      )}

      {status === 'ready' && <Markdown content={content} />}

      {status === 'ready' && (
        <div className="mt-8">
          <Link
            to={`/quiz/${cat}/${item}`}
            className="inline-block bg-primary-500 text-white px-4 py-2 rounded hover:bg-primary-600"
          >
            🎯 去做测验
          </Link>
        </div>
      )}
    </div>
  );
}
