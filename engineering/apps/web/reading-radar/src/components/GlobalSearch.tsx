// src/components/GlobalSearch.tsx
//
// MVP-6.2 全局搜索 — 头部搜索框 + 下拉结果面板，融合三类数据：
//   1. 知识点   loadAllTechItems()   匹配 title / desc / tags，
//              跳转 /learn/:cat/:itemId（保留 registry 原始 id，
//              Learn 页靠 registry id 推断 quadrant，见该页注释）
//   2. 题目     loadQuestionsByCat() 匹配 stem / scenario；保留题库
//              的 itemId 键用于深链 /quiz/:cat/:itemId
//   3. 学习内容 listLearnItems()     匹配 learn-deep 文件名 slug，
//              跳转 /learn/:cat（简化，见 task-6.2 brief）
//
// 性能：registry 解析与题库 ?raw eval 较重，仅在首次展开面板时懒加载
// 一次并做模块级缓存；listLearnItems 是内存路径过滤，随首次打开一并
// 物化。不引入模糊搜索依赖（YAGNI），中英文均为大小写不敏感的
// includes 匹配，输入经 200ms 防抖后再过滤。

import { useEffect, useRef, useState } from 'react';
import { Link } from 'react-router-dom';
import { loadAllTechItems } from '@/data/tech';
import { AVAILABLE_CATEGORIES, loadQuestionsByCat } from '@/data/questions';
import { listLearnItems } from '@/data/learn';
import type { Quadrant, Question, TechCategory, TechItem } from '@/data/types';

const DEBOUNCE_MS = 200;
const MAX_PER_GROUP = 5;

/** 雷达全部 8 个技术栈（vdb / grok 无题库与部分学习内容，查询自然为空）。 */
const TECH_CATEGORIES: readonly TechCategory[] = [
  'c',
  'cpp',
  'ds',
  'db',
  'py',
  'linux',
  'vdb',
  'grok',
];

const QUADRANTS: readonly Quadrant[] = [
  'language',
  'systems',
  'algorithms',
  'engineering',
];

/** 分类展示名，与 Learn / Home 页的卡片保持一致。 */
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

interface TechEntry {
  cat: TechCategory;
  item: TechItem;
}

interface QuestionEntry {
  cat: TechCategory;
  itemId: string;
  q: Question;
}

interface LearnEntry {
  cat: TechCategory;
  itemId: string;
}

interface SearchData {
  tech: TechEntry[];
  questions: QuestionEntry[];
  learn: LearnEntry[];
}

let cachedData: SearchData | null = null;
let inflight: Promise<SearchData> | null = null;

/**
 * 首次打开面板时构建搜索索引并缓存（模块级，跨开关/跨路由共享）。
 * 失败不缓存，下次打开自动重试。
 */
function loadSearchData(): Promise<SearchData> {
  if (cachedData) return Promise.resolve(cachedData);
  if (!inflight) {
    inflight = (async () => {
      const [grouped, ...banks] = await Promise.all([
        loadAllTechItems(),
        ...AVAILABLE_CATEGORIES.map((cat) => loadQuestionsByCat(cat)),
      ]);

      const tech: TechEntry[] = [];
      for (const cat of Object.keys(grouped) as TechCategory[]) {
        for (const item of grouped[cat]) tech.push({ cat, item });
      }

      // loadQuestionsFlat 会丢掉 itemId 分组键，而路由 /quiz/:cat/:itemId
      // 需要它 —— 因此用 loadQuestionsByCat 自己摊平。
      const questions: QuestionEntry[] = [];
      AVAILABLE_CATEGORIES.forEach((cat, i) => {
        for (const [itemId, list] of Object.entries(banks[i])) {
          for (const q of list) questions.push({ cat, itemId, q });
        }
      });

      const learn: LearnEntry[] = [];
      const seen = new Set<string>();
      for (const cat of TECH_CATEGORIES) {
        for (const quadrant of QUADRANTS) {
          for (const itemId of listLearnItems(cat, quadrant)) {
            const key = cat + '/' + itemId;
            if (seen.has(key)) continue;
            seen.add(key);
            learn.push({ cat, itemId });
          }
        }
      }

      cachedData = { tech, questions, learn };
      return cachedData;
    })().catch((err) => {
      inflight = null; // 允许下次重试
      throw err;
    });
  }
  return inflight;
}

/** 单条结果行（三类数据统一成相同渲染形状）。 */
interface ResultRow {
  to: string;
  title: string;
  subtitle: string;
}

interface SearchResults {
  tech: ResultRow[];
  question: ResultRow[];
  learn: ResultRow[];
}

function runSearch(query: string, data: SearchData): SearchResults {
  const needle = query.trim().toLowerCase();
  const out: SearchResults = { tech: [], question: [], learn: [] };
  if (!needle) return out;

  for (const { cat, item } of data.tech) {
    const hay = [item.title, item.desc, ...(item.tags ?? [])]
      .join('\n')
      .toLowerCase();
    if (!hay.includes(needle)) continue;
    out.tech.push({
      to: '/learn/' + cat + '/' + item.id,
      title: item.title,
      subtitle: CATEGORY_TITLES[cat] + ' · ' + item.desc,
    });
    if (out.tech.length >= MAX_PER_GROUP) break;
  }

  for (const { cat, itemId, q } of data.questions) {
    const hay = (q.stem + '\n' + q.scenario).toLowerCase();
    if (!hay.includes(needle)) continue;
    out.question.push({
      to: '/quiz/' + cat + '/' + itemId,
      title: q.stem || q.scenario,
      subtitle: CATEGORY_TITLES[cat] + ' / ' + itemId,
    });
    if (out.question.length >= MAX_PER_GROUP) break;
  }

  for (const { cat, itemId } of data.learn) {
    if (!itemId.toLowerCase().includes(needle)) continue;
    out.learn.push({
      to: '/learn/' + cat,
      title: itemId,
      subtitle: CATEGORY_TITLES[cat],
    });
    if (out.learn.length >= MAX_PER_GROUP) break;
  }

  return out;
}

/** 分组结果渲染（空组不渲染）。 */
function ResultGroup({
  label,
  rows,
  onPick,
}: {
  label: string;
  rows: ResultRow[];
  onPick: () => void;
}) {
  if (rows.length === 0) return null;
  return (
    <div>
      <p className="px-3 pt-2 pb-1 text-xs font-semibold text-gray-400 dark:text-gray-500">
        {label}
      </p>
      {rows.map((row, i) => (
        <Link
          key={row.to + '#' + i}
          to={row.to}
          onClick={onPick}
          className="block px-3 py-2 text-sm text-gray-700 hover:bg-gray-100 dark:text-gray-300 dark:hover:bg-gray-800"
        >
          <span className="block truncate">{row.title}</span>
          <span className="block truncate text-xs text-gray-400 dark:text-gray-500">
            {row.subtitle}
          </span>
        </Link>
      ))}
    </div>
  );
}

interface GlobalSearchProps {
  /** 追加到外层容器的类（调用方控制显隐，如 hidden md:block）。 */
  className?: string;
  /** 覆盖输入框宽度类；缺省为桌面端内联宽度。 */
  inputClassName?: string;
  /** 覆盖结果面板定位/宽度类；缺省为右对齐定宽下拉。 */
  panelClassName?: string;
  /** 展开行内的移动端实例需要自动聚焦；桌面端不抢初始焦点。 */
  autoFocus?: boolean;
}

export function GlobalSearch({
  className = '',
  inputClassName = '',
  panelClassName = '',
  autoFocus = false,
}: GlobalSearchProps) {
  const [open, setOpen] = useState(false);
  const [query, setQuery] = useState('');
  const [debounced, setDebounced] = useState('');
  const [data, setData] = useState<SearchData | null>(null);
  const [loading, setLoading] = useState(false);
  const rootRef = useRef<HTMLDivElement | null>(null);
  const inputRef = useRef<HTMLInputElement | null>(null);

  // 输入 200ms 防抖后再参与过滤
  useEffect(() => {
    const t = window.setTimeout(() => setDebounced(query), DEBOUNCE_MS);
    return () => window.clearTimeout(t);
  }, [query]);

  // 首次展开时懒加载索引（缓存命中后为同步 resolve）
  useEffect(() => {
    if (!open || data) return;
    let cancelled = false;
    setLoading(true);
    loadSearchData()
      .then((d) => {
        if (cancelled) return;
        setData(d);
        setLoading(false);
      })
      .catch((err) => {
        // eslint-disable-next-line no-console
        console.error('[GlobalSearch] 搜索索引加载失败:', err);
        if (!cancelled) setLoading(false);
      });
    return () => {
      cancelled = true;
    };
  }, [open, data]);

  // 点击面板外部关闭（与 Layout「更多」下拉同一模式）
  useEffect(() => {
    if (!open) return;
    const onDocClick = (e: MouseEvent) => {
      if (!rootRef.current) return;
      if (!rootRef.current.contains(e.target as Node)) setOpen(false);
    };
    document.addEventListener('mousedown', onDocClick);
    return () => document.removeEventListener('mousedown', onDocClick);
  }, [open]);

  const results = data ? runSearch(debounced, data) : null;
  const hasResults =
    !!results &&
    (results.tech.length > 0 ||
      results.question.length > 0 ||
      results.learn.length > 0);

  const closeAndClear = () => {
    setOpen(false);
    setQuery('');
    setDebounced('');
  };

  return (
    <div
      ref={rootRef}
      className={'relative min-w-0 ' + className}
      onKeyDown={(e) => {
        if (e.key === 'Escape') {
          setOpen(false);
          inputRef.current?.blur();
        }
      }}
    >
      <input
        ref={inputRef}
        type="search"
        value={query}
        autoFocus={autoFocus}
        onChange={(e) => {
          setQuery(e.target.value);
          setOpen(true);
        }}
        onFocus={() => setOpen(true)}
        placeholder="搜索知识点 / 题目 / 学习内容"
        aria-label="全局搜索"
        className={
          'max-w-full px-3 py-1.5 text-sm rounded-md border border-gray-300 dark:border-gray-700 bg-white dark:bg-gray-800 text-gray-900 dark:text-gray-100 placeholder:text-gray-400 focus:outline-none focus:ring-1 focus:ring-primary-500 ' +
          (inputClassName || 'w-40 md:w-56')
        }
      />

      {open && (
        <div
          role="listbox"
          aria-label="搜索结果"
          className={
            'absolute top-full mt-1 max-h-96 overflow-y-auto rounded-md shadow-lg bg-white dark:bg-gray-900 border border-gray-200 dark:border-gray-700 z-50 py-1 ' +
            (panelClassName || 'right-0 w-80')
          }
        >
          {!data || loading ? (
            <p className="px-3 py-3 text-sm text-gray-500 dark:text-gray-400">
              搜索索引加载中…
            </p>
          ) : !debounced.trim() ? (
            <p className="px-3 py-3 text-sm text-gray-500 dark:text-gray-400">
              输入关键字，搜索知识点、题目与学习内容。
            </p>
          ) : !hasResults ? (
            <p className="px-3 py-3 text-sm text-gray-500 dark:text-gray-400">
              未找到与「{debounced.trim()}」相关的结果。
            </p>
          ) : (
            <>
              <ResultGroup
                label="知识点"
                rows={results.tech}
                onPick={closeAndClear}
              />
              <ResultGroup
                label="题目"
                rows={results.question}
                onPick={closeAndClear}
              />
              <ResultGroup
                label="学习内容"
                rows={results.learn}
                onPick={closeAndClear}
              />
            </>
          )}
        </div>
      )}
    </div>
  );
}
