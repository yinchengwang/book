// src/pages/Interview/index.tsx
//
// MVP-6.1 Interview — categorized Q&A library.
//
// Data: `data/interview/interview-questions.js` (CommonJS const declarations,
// not an ESM module). Imported as `?raw` and evaluated in a Function body, the
// same pattern used by `@/data/tech.ts`.
//
// The JS file exposes three top-level consts:
//   INTERVIEW_CATEGORIES — top-level groups (language / db / ...)
//   INTERVIEW_STACKS     — stacks per category (cpp-language / mysql / ...)
//   INTERVIEW_QUESTIONS  — flat list of Q&A entries; each entry has
//     { id, stack, chapter, difficulty, priority, tags, title, body }
// `body` is markdown.
//
// MVP scope:
//   - Tabs: stacks (C++, MySQL, ...). Categories are derived.
//   - Each stack: list of questions, click to expand body.
//   - Body rendered through `@shared/components/Markdown`.

import { useMemo, useState } from 'react';
import { Card } from '@shared/ui/Card';
import { Markdown } from '@shared/components/Markdown';
import interviewSource from '@data/interview/interview-questions.js?raw';

interface InterviewCategory {
  id: string;
  label: string;
  icon: string;
}
interface InterviewStack {
  id: string;
  label: string;
  icon: string;
  category: string;
}
interface InterviewQuestion {
  id: string;
  stack: string;
  chapter: string;
  difficulty: string;
  priority: string;
  tags: string[];
  title: string;
  body: string;
}

interface InterviewBundle {
  categories: InterviewCategory[];
  stacks: InterviewStack[];
  questions: InterviewQuestion[];
}

/** Lazily parse the legacy CommonJS script and expose the three consts. */
function parseInterviewSource(source: string): InterviewBundle {
  try {
    const body = `${source}\nreturn { categories: INTERVIEW_CATEGORIES, stacks: INTERVIEW_STACKS, questions: INTERVIEW_QUESTIONS };`;
    // eslint-disable-next-line @typescript-eslint/no-implied-eval
    const fn = new Function(body);
    return fn() as InterviewBundle;
  } catch (err) {
    // eslint-disable-next-line no-console
    console.error('[Interview] failed to parse interview-questions.js:', err);
    return { categories: [], stacks: [], questions: [] };
  }
}

const bundle: InterviewBundle = parseInterviewSource(interviewSource);

const DIFFICULTY_COLORS: Record<string, string> = {
  easy: 'bg-emerald-100 text-emerald-700 dark:bg-emerald-900/40 dark:text-emerald-300',
  medium: 'bg-amber-100 text-amber-700 dark:bg-amber-900/40 dark:text-amber-300',
  hard: 'bg-rose-100 text-rose-700 dark:bg-rose-900/40 dark:text-rose-300',
};

export function Interview() {
  const stacks = bundle.stacks;
  const [activeStack, setActiveStack] = useState<string>(stacks[0]?.id ?? '');
  const [expandedId, setExpandedId] = useState<string | null>(null);

  const questions = useMemo(
    () => bundle.questions.filter((q) => q.stack === activeStack),
    [activeStack]
  );

  if (stacks.length === 0) {
    return (
      <div className="max-w-4xl mx-auto py-8">
        <h1 className="text-2xl font-bold mb-4">💬 面试题</h1>
        <Card className="p-6 text-center text-gray-500 dark:text-gray-400">
          暂无面试题数据
        </Card>
      </div>
    );
  }

  return (
    <div className="max-w-5xl mx-auto space-y-4">
      <div>
        <h1 className="text-2xl font-bold mb-1">💬 面试题库</h1>
        <p className="text-sm text-gray-500 dark:text-gray-400">
          按技术栈分类 · 点击展开查看回答
        </p>
      </div>

      {/* Stack tabs */}
      <div className="flex gap-2 flex-wrap border-b border-gray-200 dark:border-gray-700 pb-3">
        {stacks.map((s) => (
          <button
            key={s.id}
            type="button"
            onClick={() => {
              setActiveStack(s.id);
              setExpandedId(null);
            }}
            className={`px-3 py-1.5 rounded-md text-sm font-medium transition-colors ${
              activeStack === s.id
                ? 'bg-primary-500 text-white'
                : 'bg-gray-100 text-gray-700 hover:bg-gray-200 dark:bg-gray-800 dark:text-gray-300 dark:hover:bg-gray-700'
            }`}
          >
            <span className="mr-1">{s.icon}</span>
            {s.label}
          </button>
        ))}
      </div>

      {/* Question list */}
      {questions.length === 0 ? (
        <Card className="p-6 text-center text-gray-500 dark:text-gray-400">
          该分类暂无题目
        </Card>
      ) : (
        <div className="space-y-3">
          {questions.map((q) => {
            const open = expandedId === q.id;
            return (
              <Card key={q.id} className="overflow-hidden">
                <button
                  type="button"
                  onClick={() => setExpandedId(open ? null : q.id)}
                  className="w-full text-left p-4 hover:bg-gray-50 dark:hover:bg-gray-800/60 transition-colors"
                >
                  <div className="flex items-start justify-between gap-3">
                    <div className="flex-1 min-w-0">
                      <h3 className="font-semibold text-gray-900 dark:text-gray-100">
                        {q.title}
                      </h3>
                      <div className="flex flex-wrap items-center gap-2 mt-1.5 text-xs">
                        <span
                          className={`px-1.5 py-0.5 rounded ${DIFFICULTY_COLORS[q.difficulty] ?? 'bg-gray-100 text-gray-600'}`}
                        >
                          {q.difficulty}
                        </span>
                        {q.priority && (
                          <span className="text-gray-500 dark:text-gray-400">
                            优先级: {q.priority}
                          </span>
                        )}
                        {q.tags.slice(0, 3).map((t) => (
                          <span
                            key={t}
                            className="text-gray-500 dark:text-gray-400 opacity-70"
                          >
                            #{t}
                          </span>
                        ))}
                      </div>
                    </div>
                    <span className="text-gray-400 shrink-0 mt-1">
                      {open ? '▲' : '▼'}
                    </span>
                  </div>
                </button>
                {open && (
                  <div className="px-4 pb-4 pt-2 border-t border-gray-100 dark:border-gray-700">
                    <Markdown content={q.body} />
                  </div>
                )}
              </Card>
            );
          })}
        </div>
      )}
    </div>
  );
}
