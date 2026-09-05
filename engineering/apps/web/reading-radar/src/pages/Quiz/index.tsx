// src/pages/Quiz/index.tsx
//
// MVP-5.2 minimal quiz runner.
//
// Routes:
//   /quiz                 — landing (no questions loaded, prompts to pick a topic)
//   /quiz/:cat/:item      — runs questions for that (category, itemId) pair
//
// Behavior:
//   - Calls `loadQuestions(cat, item)` to fetch questions.
//   - Renders one question at a time via QuestionCard.
//   - Click an option → marks answered, shows feedback + explanation.
//   - Click "下一题" → advances; after the last question, shows total score.
import { useEffect, useMemo, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { Button } from '@shared/ui/Button';
import { Card } from '@shared/ui/Card';
import { QuestionCard } from '@/components/Quiz/QuestionCard';
import { loadQuestions, AVAILABLE_CATEGORIES } from '@/data/questions';
import type { Question, TechCategory } from '@/data/types';

type Status = 'idle' | 'loading' | 'ready' | 'error';

export function Quiz() {
  const { cat, item } = useParams();

  const isQuizRoute = Boolean(cat && item);
  const typedCat: TechCategory | undefined = useMemo(() => {
    if (!cat) return undefined;
    return (AVAILABLE_CATEGORIES as readonly string[]).includes(cat)
      ? (cat as TechCategory)
      : undefined;
  }, [cat]);

  const [status, setStatus] = useState<Status>('idle');
  const [error, setError] = useState<string | null>(null);
  const [questions, setQuestions] = useState<Question[]>([]);
  const [idx, setIdx] = useState(0);
  const [score, setScore] = useState({ correct: 0, answered: 0 });
  const [done, setDone] = useState(false);

  // Reset state whenever the route's (cat, item) changes — important
  // when the user navigates between quiz topics without unmounting.
  useEffect(() => {
    setIdx(0);
    setScore({ correct: 0, answered: 0 });
    setDone(false);
    setError(null);

    if (!isQuizRoute || !typedCat || !item) {
      setStatus('idle');
      setQuestions([]);
      return;
    }

    let cancelled = false;
    setStatus('loading');
    loadQuestions(typedCat, item)
      .then((qs) => {
        if (cancelled) return;
        setQuestions(qs);
        setStatus('ready');
      })
      .catch((err: unknown) => {
        if (cancelled) return;
        setError(err instanceof Error ? err.message : String(err));
        setStatus('error');
      });
    return () => {
      cancelled = true;
    };
  }, [isQuizRoute, typedCat, item]);

  function handleAnswer(_opt: string, isCorrect: boolean) {
    setScore((s) => ({
      correct: s.correct + (isCorrect ? 1 : 0),
      answered: s.answered + 1,
    }));
  }

  function goNext() {
    if (idx + 1 >= questions.length) {
      setDone(true);
    } else {
      setIdx((i) => i + 1);
    }
  }

  function restart() {
    setIdx(0);
    setScore({ correct: 0, answered: 0 });
    setDone(false);
  }

  // --- Landing (no /:cat/:item in URL) -----------------------------
  if (!isQuizRoute) {
    return (
      <div className="max-w-4xl mx-auto space-y-4">
        <h1 className="text-2xl font-bold">📝 测评</h1>
        <Card className="p-6">
          <p className="text-gray-700 dark:text-gray-300 mb-3">
            MVP-5.2 极简题库：从左侧导航进入「学习」选一个知识点，再用其链接进入对应测评。
          </p>
          <p className="text-sm text-gray-500 dark:text-gray-400">
            直接访问形如 <code>/quiz/c/pointer</code> 的路径即可开始作答。
          </p>
          <div className="mt-4 flex gap-2">
            <Link to="/learn/c/pointer">
              <Button variant="primary">前往 C · pointer 学习页</Button>
            </Link>
            <Link to="/">
              <Button variant="ghost">返回首页</Button>
            </Link>
          </div>
        </Card>
      </div>
    );
  }

  // --- Loading ------------------------------------------------------
  if (status === 'loading' || status === 'idle') {
    return (
      <div className="max-w-4xl mx-auto">
        <p className="text-gray-600 dark:text-gray-400">加载题目…</p>
      </div>
    );
  }

  // --- Error --------------------------------------------------------
  if (status === 'error') {
    return (
      <div className="max-w-4xl mx-auto space-y-3">
        <h1 className="text-2xl font-bold">📝 测评</h1>
        <Card className="p-6">
          <p className="text-red-600 dark:text-red-400">
            题目加载失败：{error ?? '未知错误'}
          </p>
          <p className="text-sm text-gray-500 dark:text-gray-400 mt-2">
            分类 <code>{cat}</code> / 知识点 <code>{item}</code>
          </p>
        </Card>
      </div>
    );
  }

  // --- Empty -------------------------------------------------------
  if (questions.length === 0) {
    return (
      <div className="max-w-4xl mx-auto space-y-3">
        <h1 className="text-2xl font-bold">📝 测评</h1>
        <Card className="p-6">
          <p className="text-gray-700 dark:text-gray-300">
            该知识点暂无可用题目。
          </p>
          <p className="text-sm text-gray-500 dark:text-gray-400 mt-2">
            <code>{cat}</code> / <code>{item}</code>
          </p>
          <div className="mt-4">
            <Link to="/learn">
              <Button variant="ghost">返回学习列表</Button>
            </Link>
          </div>
        </Card>
      </div>
    );
  }

  // --- Done --------------------------------------------------------
  if (done) {
    const pct = Math.round((score.correct / questions.length) * 100);
    return (
      <div className="max-w-4xl mx-auto space-y-4">
        <h1 className="text-2xl font-bold">🎉 完成</h1>
        <Card className="p-6">
          <p className="text-lg mb-2">
            得分：<span className="font-bold text-primary-600 dark:text-primary-400">{score.correct}</span>
            {' / '}
            {questions.length}（{pct}%）
          </p>
          <p className="text-sm text-gray-500 dark:text-gray-400 mb-4">
            分类 <code>{cat}</code> / 知识点 <code>{item}</code>
          </p>
          <div className="flex gap-2">
            <Button variant="primary" onClick={restart}>
              再来一次
            </Button>
            <Link to="/learn">
              <Button variant="ghost">返回学习</Button>
            </Link>
          </div>
        </Card>
      </div>
    );
  }

  // --- Active quiz -------------------------------------------------
  const current = questions[idx];
  const allAnswered = score.answered >= idx + 1;

  return (
    <div className="max-w-4xl mx-auto space-y-4">
      <div className="flex items-center justify-between text-sm text-gray-500 dark:text-gray-400">
        <span>
          测评 · <code>{cat}</code> / <code>{item}</code>
        </span>
        <span>
          已答 {score.answered} / {questions.length} · 正确 {score.correct}
        </span>
      </div>
      <QuestionCard
        question={current}
        index={idx}
        total={questions.length}
        onAnswer={handleAnswer}
      />
      <div className="flex justify-end">
        <Button variant="primary" onClick={goNext} disabled={!allAnswered}>
          {idx + 1 >= questions.length ? '完成' : '下一题'}
        </Button>
      </div>
    </div>
  );
}
