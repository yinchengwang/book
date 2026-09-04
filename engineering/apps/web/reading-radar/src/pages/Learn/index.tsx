// src/pages/Learn/index.tsx
import { useParams } from 'react-router-dom';

export function Learn() {
  const { cat, item } = useParams();

  return (
    <div className="max-w-4xl mx-auto">
      <h1 className="text-2xl font-bold mb-4">📖 学习</h1>
      <p className="text-gray-600 dark:text-gray-400 mb-2">
        知识详情 — 选择知识点（开发中）
      </p>
      {cat && (
        <p className="text-sm text-gray-500 dark:text-gray-400">
          当前分类：<code className="px-1 py-0.5 bg-gray-100 dark:bg-gray-800 rounded">{cat}</code>
          {item && (
            <>
              {' / '}知识点：<code className="px-1 py-0.5 bg-gray-100 dark:bg-gray-800 rounded">{item}</code>
            </>
          )}
        </p>
      )}
    </div>
  );
}