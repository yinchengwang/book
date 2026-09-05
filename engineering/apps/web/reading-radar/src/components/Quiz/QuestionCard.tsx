// src/components/Quiz/QuestionCard.tsx
//
// Single-question display + click-to-answer.
// The Quiz page owns scoring / navigation; this component is presentational
// plus the immediate "right/wrong + explanation" feedback after each click.
import { useState } from 'react';
import { Card } from '@shared/ui/Card';
import { Markdown } from '@shared/components/Markdown';
import type { Question } from '@/data/types';

export interface QuestionCardProps {
  question: Question;
  index: number;
  total: number;
  onAnswer: (opt: string, isCorrect: boolean) => void;
}

function answerMatches(q: Question, opt: string): boolean {
  if (Array.isArray(q.answer)) {
    return q.answer.some((a) => opt.startsWith(a));
  }
  if (typeof q.answer === 'boolean') {
    // True/false questions: options may be a single "True" / "False" label
    // (no letter prefix). Map truthiness to the first letter of the option.
    if (q.answer) return /^(对|true|t|yes|y)/i.test(opt.trim());
    return /^(错|false|f|no|n)/i.test(opt.trim());
  }
  // q.answer is a letter like 'A' / 'B' / 'C' / 'D'; options look like
  // 'A. xxx'. Prefix-match is the cheapest correct comparison.
  return opt.startsWith(q.answer);
}

export function QuestionCard({ question: q, index, total, onAnswer }: QuestionCardProps) {
  const [picked, setPicked] = useState<string | null>(null);

  const isAnswered = picked !== null;
  const pickedCorrect = isAnswered && answerMatches(q, picked);

  function handlePick(opt: string) {
    if (isAnswered) return;
    setPicked(opt);
    onAnswer(opt, answerMatches(q, opt));
  }

  return (
    <Card className="p-6">
      <div className="flex items-center justify-between text-xs text-gray-500 dark:text-gray-400 mb-3">
        <span>
          题目 {index + 1} / {total}
        </span>
        <span>
          {q.quadrant ?? '—'} · {q.ring ?? '—'} · {q.difficulty}
        </span>
      </div>

      {q.scenario && (
        <p className="text-sm text-gray-600 dark:text-gray-400 italic mb-3">
          {q.scenario}
        </p>
      )}

      <div className="text-base font-medium mb-4">
        <Markdown content={q.stem} />
      </div>

      {q.code && (
        <pre className="bg-gray-50 dark:bg-gray-900 border border-gray-200 dark:border-gray-700 rounded-md p-3 overflow-x-auto text-sm font-mono mb-4">
          <code>{q.code}</code>
        </pre>
      )}

      {q.options.length > 0 ? (
        <ul className="space-y-2 mb-4">
          {q.options.map((opt, i) => {
            const correct = answerMatches(q, opt);
            const isPicked = picked === opt;
            const stateClass = !isAnswered
              ? 'hover:border-primary-400 hover:bg-primary-50 dark:hover:bg-primary-900/20 cursor-pointer'
              : correct
                ? 'border-green-500 bg-green-50 dark:bg-green-900/20'
                : isPicked
                  ? 'border-red-500 bg-red-50 dark:bg-red-900/20'
                  : 'opacity-60';

            return (
              <li key={i}>
                <button
                  type="button"
                  onClick={() => handlePick(opt)}
                  disabled={isAnswered}
                  className={`w-full text-left px-4 py-3 rounded-md border transition-colors ${stateClass} border-gray-200 dark:border-gray-700`}
                >
                  <span className="font-mono mr-2">{opt.split('.')[0]}.</span>
                  <span>{opt.replace(/^[A-D]\.\s*/, '')}</span>
                </button>
              </li>
            );
          })}
        </ul>
      ) : (
        <div className="text-sm text-gray-500 dark:text-gray-400 mb-4">
          （该题无选项，参考答案为：{String(q.answer)}）
        </div>
      )}

      {isAnswered && (
        <div
          className={`mt-4 p-3 rounded-md border ${
            pickedCorrect
              ? 'border-green-300 bg-green-50 dark:bg-green-900/20 dark:border-green-700'
              : 'border-red-300 bg-red-50 dark:bg-red-900/20 dark:border-red-700'
          }`}
        >
          <div className={`text-sm font-semibold mb-1 ${pickedCorrect ? 'text-green-700 dark:text-green-300' : 'text-red-700 dark:text-red-300'}`}>
            {pickedCorrect ? '✓ 回答正确' : '✗ 回答错误'}
          </div>
          <div className="text-sm">
            <span className="font-medium">解析：</span>
            <Markdown content={q.explanation} />
          </div>
        </div>
      )}
    </Card>
  );
}
