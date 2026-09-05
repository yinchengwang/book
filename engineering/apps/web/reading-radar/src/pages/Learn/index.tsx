// src/pages/Learn/index.tsx
//
// MVP-5.3 Learn page — renders a knowledge point's deep-dive Markdown.
//
// Route:
//   /learn/:cat/:item  — spec routing; URL provides (cat, itemId), quadrant
//                        is inferred from the tech-items registry (see below).
//   /learn             — landing (no topic picked yet)
//
// Data flow:
//   1. Read cat + item from useParams().
//   2. Look the item up in the tech registry to recover its `quadrant`
//      (the route omits it). Fall back to 'language' if not found.
//   3. Call `loadLearnContent(cat, quadrant, itemId)` to get the raw
//      Markdown source.
//   4. Render through the cross-project `@shared/components/Markdown`
//      component (created in Task 5.2; do NOT recreate locally).
//
// Notes:
//   - The shared Markdown component already applies the `prose` classes,
//     so this page only needs to manage loading / error / content layout.
//   - The quiz link uses the matching `/quiz/:cat/:item` route.

import { useEffect, useMemo, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { Markdown } from '@shared/components/Markdown';
import { loadAllTechItems } from '@/data/tech';
import { loadLearnContent } from '@/data/learn';
import { AVAILABLE_CATEGORIES } from '@/data/questions';
import type { Quadrant, TechCategory } from '@/data/types';

type Status = 'idle' | 'loading' | 'ready' | 'error';

/**
 * Default quadrant when the item can't be found in the tech registry.
 * 'language' is the safest default — it's the most common quadrant
 * for C-family basics like 'syntax', 'types', etc.
 */
const DEFAULT_QUADRANT: Quadrant = 'language';

export function Learn() {
  const { cat, item } = useParams();

  // Route guard — typed narrow for downstream data calls.
  const isLearnRoute = Boolean(cat && item);
  const typedCat: TechCategory | undefined = useMemo(() => {
    if (!cat) return undefined;
    return (AVAILABLE_CATEGORIES as readonly string[]).includes(cat)
      ? (cat as TechCategory)
      : undefined;
  }, [cat]);

  const [status, setStatus] = useState<Status>('idle');
  const [error, setError] = useState<string | null>(null);
  const [content, setContent] = useState<string>('');

  useEffect(() => {
    // Reset whenever the (cat, item) pair changes — handles in-place
    // navigation between topics without unmounting the page.
    setError(null);
    setContent('');

    if (!isLearnRoute || !typedCat || !item) {
      setStatus('idle');
      return;
    }

    let cancelled = false;
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

        const md = await loadLearnContent(typedCat, quadrant, item);
        if (cancelled) return;
        setContent(md);
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
  }, [isLearnRoute, typedCat, item]);

  // --- Landing (no /:cat/:item in URL) --------------------------
  if (!isLearnRoute) {
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

  // --- Topic view (/learn/:cat/:item) ---------------------------
  return (
    <div className="max-w-4xl mx-auto">
      <div className="mb-4 text-sm text-gray-500 dark:text-gray-400">
        当前分类：
        <code className="px-1 py-0.5 bg-gray-100 dark:bg-gray-800 rounded">
          {cat}
        </code>
        {' / '}知识点：
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
          {error && (
            <p className="text-sm text-red-600 dark:text-red-400 mt-1">
              {error}
            </p>
          )}
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