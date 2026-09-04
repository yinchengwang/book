import { Link } from 'react-router-dom';

export function NotFound() {
  return (
    <div className="max-w-2xl mx-auto text-center py-16">
      <h1 className="text-4xl font-bold mb-4">404</h1>
      <p className="text-gray-600 dark:text-gray-400 mb-6">页面未找到</p>
      <Link to="/" className="text-primary-500 hover:underline">← 返回首页</Link>
    </div>
  );
}
