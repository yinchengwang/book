// src/pages/Practice/index.tsx
//
// MVP-6.1 Practice — quick-entry hub to category quizzes.
//
// No dedicated data file exists. The legacy practice.html scrapes quiz banks
// to build a category list. For MVP we hardcode the same 7 quiz categories
// (no `grok`) the Learn page exposes, so users can jump straight into the
// quiz tab for a stack.

import { Link } from 'react-router-dom';
import { Card } from '@shared/ui/Card';

interface PracticeCat {
  key: string;
  title: string;
  desc: string;
  emoji: string;
}

const CATEGORIES: PracticeCat[] = [
  { key: 'c', title: 'C 语言', desc: '指针 / 内存 / 文件 IO', emoji: '🔧' },
  { key: 'cpp', title: 'C++', desc: '面向对象 / STL / 模板', emoji: '⚙️' },
  { key: 'ds', title: '数据结构', desc: '链表 / 树 / 图算法', emoji: '🌲' },
  { key: 'db', title: '数据库', desc: 'SQL / 事务 / 索引', emoji: '🗄️' },
  { key: 'py', title: 'Python', desc: '语法 / 标准库 / 生态', emoji: '🐍' },
  { key: 'linux', title: 'Linux', desc: 'Shell / 进程 / 网络', emoji: '🐧' },
  { key: 'vdb', title: '向量库', desc: 'Milvus / pgvector', emoji: '🧮' },
];

export function Practice() {
  return (
    <div className="max-w-5xl mx-auto space-y-4">
      <div>
        <h1 className="text-2xl font-bold mb-1">🎯 练习中心</h1>
        <p className="text-sm text-gray-500 dark:text-gray-400">
          选择分类，开始刷题
        </p>
      </div>

      <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 gap-4">
        {CATEGORIES.map((c) => (
          <Link key={c.key} to={`/quiz/${c.key}`} className="block">
            <Card className="p-4 h-full hover:shadow-md transition-shadow">
              <div className="flex items-start gap-3">
                <span className="text-2xl shrink-0">{c.emoji}</span>
                <div className="min-w-0">
                  <h3 className="font-semibold text-gray-900 dark:text-gray-100">
                    {c.title}
                  </h3>
                  <p className="text-sm text-gray-500 dark:text-gray-400 mt-0.5">
                    {c.desc}
                  </p>
                </div>
              </div>
            </Card>
          </Link>
        ))}
      </div>
    </div>
  );
}
