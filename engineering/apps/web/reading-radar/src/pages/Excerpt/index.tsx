// src/pages/Excerpt/index.tsx
//
// MVP-6.1 Excerpt — book notes / quote browser.
//
// Data: every `.md` file under `data/excerpt/**`. Frontmatter shape:
//   ---
//   book: "..."
//   date: "..."    (year, or year-month-day)
//   tags: [...]
//   ---
// Body is the actual note text.
//
// Eagerly raw-loaded via import.meta.glob so listing is fast.

import { useMemo, useState } from 'react';
import { Card } from '@shared/ui/Card';
import { Markdown } from '@shared/components/Markdown';

interface Frontmatter {
  book?: string;
  date?: string;
  tags?: string[];
}

interface ExcerptEntry {
  path: string;
  group: string; // year or "reading_record"
  book: string;
  date?: string;
  tags: string[];
  content: string;
  excerpt: string; // first 80 chars for the card preview
}

// eager: true → values are the raw file contents synchronously (Record<string, string>).
// Without it, values are async loaders () => Promise<string> and would crash
// the sync parse below at module init (white-screening every route).
const rawFiles = import.meta.glob<string>('@data/excerpt/**/*.md', {
  query: '?raw',
  import: 'default',
  eager: true,
});

/** Parse a minimal YAML-ish frontmatter block (`---\n...\n---`). */
function parseFrontmatter(text: string): { meta: Frontmatter; body: string } {
  const m = /^---\s*\n([\s\S]*?)\n---\s*\n?/.exec(text);
  if (!m) return { meta: {}, body: text };
  const block = m[1];
  const meta: Frontmatter = {};
  for (const line of block.split('\n')) {
    const kv = /^([a-zA-Z_]+)\s*:\s*(.*)$/.exec(line.trim());
    if (!kv) continue;
    const key = kv[1];
    let val: string | string[] = kv[2].trim();
    // Strip surrounding quotes.
    if ((val.startsWith('"') && val.endsWith('"')) || (val.startsWith("'") && val.endsWith("'"))) {
      val = val.slice(1, -1);
    }
    if (key === 'tags') {
      // Extract [a, b, c] form.
      const tm = /\[([^\]]*)\]/.exec(val);
      if (tm) {
        meta.tags = tm[1]
          .split(',')
          .map((s) => s.trim().replace(/^["']|["']$/g, ''))
          .filter(Boolean);
        continue;
      }
      meta.tags = [];
      continue;
    }
    if (key === 'book') meta.book = val as string;
    if (key === 'date') meta.date = val as string;
  }
  return { meta, body: text.slice(m[0].length) };
}

function truncate(text: string, max = 100): string {
  const flat = text.replace(/\s+/g, ' ').trim();
  return flat.length <= max ? flat : `${flat.slice(0, max)}…`;
}

const entries: ExcerptEntry[] = (() => {
  const list: ExcerptEntry[] = [];
  for (const [path, raw] of Object.entries(rawFiles)) {
    // path looks like: /data/excerpt/2025/c++新经典.md
    // or nested: /data/excerpt/reading_record/10x程序员工作法/00.md
    // The first segment after /excerpt/ is the group; allow any depth below it.
    const m = /\/excerpt\/([^/]+)\/(?:[^/]+\/)*[^/]+\.md$/.exec(path);
    const group = m ? m[1] : 'unknown';
    // Path-based fallback book name = filename without ext.
    const fname = path.replace(/^.*\//, '').replace(/\.md$/, '');
    // eager glob: raw is already the file content string.
    const text = typeof raw === 'string' ? raw : '';
    const { meta, body } = parseFrontmatter(text);
    list.push({
      path,
      group,
      book: meta.book ?? fname,
      date: meta.date ?? group,
      tags: meta.tags ?? [],
      content: body,
      excerpt: truncate(body),
    });
  }
  // Newest first.
  return list.sort((a, b) => (b.date ?? '').localeCompare(a.date ?? ''));
})();

const GROUP_LABEL: Record<string, string> = {
  '2024': '2024 年',
  '2025': '2025 年',
  reading_record: '长期记录',
};

export function Excerpt() {
  const [openPath, setOpenPath] = useState<string | null>(null);
  const groups = useMemo(() => {
    const map = new Map<string, ExcerptEntry[]>();
    for (const e of entries) {
      if (!map.has(e.group)) map.set(e.group, []);
      map.get(e.group)!.push(e);
    }
    return Array.from(map.entries());
  }, []);

  if (entries.length === 0) {
    return (
      <div className="max-w-4xl mx-auto py-8">
        <h1 className="text-2xl font-bold mb-4">📖 读书摘录</h1>
        <Card className="p-6 text-center text-gray-500 dark:text-gray-400">
          暂无摘录数据
        </Card>
      </div>
    );
  }

  return (
    <div className="max-w-5xl mx-auto space-y-6">
      <div>
        <h1 className="text-2xl font-bold mb-1">📖 读书摘录</h1>
        <p className="text-sm text-gray-500 dark:text-gray-400">
          {entries.length} 条笔记 · 按年份分组
        </p>
      </div>

      {groups.map(([group, list]) => (
        <section key={group}>
          <h2 className="text-lg font-semibold text-gray-900 dark:text-gray-100 mb-2">
            {GROUP_LABEL[group] ?? group}{' '}
            <span className="text-sm font-normal text-gray-500 dark:text-gray-400">
              ({list.length})
            </span>
          </h2>
          <div className="space-y-2">
            {list.map((e) => {
              const open = openPath === e.path;
              return (
                <Card key={e.path} className="overflow-hidden">
                  <button
                    type="button"
                    onClick={() => setOpenPath(open ? null : e.path)}
                    className="w-full text-left p-4 hover:bg-gray-50 dark:hover:bg-gray-800/60 transition-colors"
                  >
                    <div className="flex items-start justify-between gap-3">
                      <div className="min-w-0 flex-1">
                        <h3 className="font-medium text-gray-900 dark:text-gray-100 truncate">
                          📕 {e.book}
                        </h3>
                        <p className="text-sm text-gray-600 dark:text-gray-400 mt-1 line-clamp-2">
                          {e.excerpt}
                        </p>
                        <div className="flex flex-wrap gap-1 mt-2">
                          {e.tags.map((t) => (
                            <span
                              key={t}
                              className="text-xs px-1.5 py-0.5 rounded bg-gray-100 text-gray-600 dark:bg-gray-800 dark:text-gray-400"
                            >
                              #{t}
                            </span>
                          ))}
                          {e.date && (
                            <span className="text-xs text-gray-400 dark:text-gray-500">
                              · {e.date}
                            </span>
                          )}
                        </div>
                      </div>
                      <span className="text-gray-400 shrink-0 mt-1">
                        {open ? '▲' : '▼'}
                      </span>
                    </div>
                  </button>
                  {open && (
                    <div className="px-4 pb-4 pt-2 border-t border-gray-100 dark:border-gray-700">
                      <Markdown content={e.content} />
                    </div>
                  )}
                </Card>
              );
            })}
          </div>
        </section>
      ))}
    </div>
  );
}
