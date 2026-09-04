// src/pages/Home/index.tsx
/**
 * Home page — Knowledge map entry.
 * MVP-5.1 enhancement: 8 stack cards link to /learn/:cat for early navigation.
 * Not in original spec; kept because it provides discoverability.
 */
import { Link } from 'react-router-dom';

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

export function Home() {
  return (
    <div className="max-w-6xl mx-auto">
      <h1 className="text-3xl font-bold mb-2">📚 知识地图</h1>
      <p className="text-gray-600 dark:text-gray-400 mb-6">选择学习领域，进入雷达图查看知识点分布。</p>

      <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-4 gap-4">
        {stacks.map((s) => (
          <Link
            key={s.key}
            to={`/learn/${s.key}`}
            className="p-4 bg-white dark:bg-gray-800 rounded-lg shadow hover:shadow-lg transition-shadow block border border-gray-100 dark:border-gray-700"
          >
            <h2 className="text-lg font-semibold">{s.title}</h2>
            <p className="text-sm text-gray-500 dark:text-gray-400 mt-1">{s.desc}</p>
          </Link>
        ))}
      </div>
    </div>
  );
}