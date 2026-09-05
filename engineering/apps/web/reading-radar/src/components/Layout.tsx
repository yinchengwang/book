// src/components/Layout.tsx
import { useEffect, useRef, useState } from 'react';
import { NavLink, Outlet } from 'react-router-dom';
import { useTheme } from '@shared/theme/ThemeProvider';

const navItems = [
  { to: '/quiz', label: '测评' },
  { to: '/learn', label: '学习' },
  { to: '/kanban', label: '看板' },
  { to: '/dashboard', label: '仪表盘' },
];

// "更多" 下拉菜单（MVP-6.1 引入）—— 收纳次要页面，避免顶部导航过挤。
const moreItems = [
  { to: '/five-year-plan', label: '🏗️ 五年计划' },
  { to: '/interview', label: '💬 面试题' },
  { to: '/interview-tracker', label: '🏢 面试追踪' },
  { to: '/practice', label: '🎯 练习' },
  { to: '/grok', label: '🧠 Grok 题库' },
  { to: '/excerpt', label: '📖 读书摘录' },
];

export function Layout() {
  const { theme, toggle } = useTheme();
  const [moreOpen, setMoreOpen] = useState(false);
  const moreRef = useRef<HTMLDivElement | null>(null);

  // 点外面关闭下拉
  useEffect(() => {
    if (!moreOpen) return;
    const onDocClick = (e: MouseEvent) => {
      if (!moreRef.current) return;
      if (!moreRef.current.contains(e.target as Node)) setMoreOpen(false);
    };
    document.addEventListener('mousedown', onDocClick);
    return () => document.removeEventListener('mousedown', onDocClick);
  }, [moreOpen]);

  return (
    <div className="min-h-screen flex flex-col">
      <header className="border-b border-gray-200 dark:border-gray-800 bg-white dark:bg-gray-900">
        <div className="max-w-6xl mx-auto px-4 py-3 flex items-center justify-between">
          <NavLink to="/" className="flex items-center gap-2 text-xl font-bold">
            <span aria-hidden="true">📚</span>
            <span>Reading Radar</span>
          </NavLink>

          <nav className="flex items-center gap-1">
            {navItems.map((item) => (
              <NavLink
                key={item.to}
                to={item.to}
                className={({ isActive }) =>
                  [
                    'px-3 py-2 rounded-md text-sm transition-colors',
                    isActive
                      ? 'bg-primary-50 text-primary-900 dark:bg-primary-900 dark:text-primary-50'
                      : 'text-gray-700 hover:bg-gray-100 dark:text-gray-300 dark:hover:bg-gray-800',
                  ].join(' ')
                }
              >
                {item.label}
              </NavLink>
            ))}

            {/* 更多下拉 */}
            <div className="relative" ref={moreRef}>
              <button
                type="button"
                onClick={() => setMoreOpen((o) => !o)}
                aria-haspopup="menu"
                aria-expanded={moreOpen}
                className={`px-3 py-2 rounded-md text-sm transition-colors ${
                  moreOpen
                    ? 'bg-gray-100 text-gray-900 dark:bg-gray-800 dark:text-gray-100'
                    : 'text-gray-700 hover:bg-gray-100 dark:text-gray-300 dark:hover:bg-gray-800'
                }`}
              >
                更多 ▾
              </button>
              {moreOpen && (
                <div
                  role="menu"
                  className="absolute right-0 mt-1 w-48 rounded-md shadow-lg bg-white dark:bg-gray-900 border border-gray-200 dark:border-gray-700 z-50 py-1"
                >
                  {moreItems.map((item) => (
                    <NavLink
                      key={item.to}
                      to={item.to}
                      onClick={() => setMoreOpen(false)}
                      role="menuitem"
                      className={({ isActive }) =>
                        [
                          'block px-3 py-2 text-sm transition-colors',
                          isActive
                            ? 'bg-primary-50 text-primary-900 dark:bg-primary-900 dark:text-primary-50'
                            : 'text-gray-700 hover:bg-gray-100 dark:text-gray-300 dark:hover:bg-gray-800',
                        ].join(' ')
                      }
                    >
                      {item.label}
                    </NavLink>
                  ))}
                </div>
              )}
            </div>

            <button
              type="button"
              onClick={toggle}
              aria-label="Toggle theme"
              className="ml-2 px-3 py-2 rounded-md text-sm hover:bg-gray-100 dark:hover:bg-gray-800"
            >
              {theme === 'dark' ? '☀️' : '🌙'}
            </button>
          </nav>
        </div>
      </header>

      <main className="flex-1 px-4 py-6">
        <Outlet />
      </main>

      <footer className="border-t border-gray-200 dark:border-gray-800 py-4">
        <div className="max-w-6xl mx-auto px-4 text-xs text-gray-500 dark:text-gray-400">
          Reading Radar · MVP-6.1
        </div>
      </footer>
    </div>
  );
}
