// src/components/Layout.tsx
import { NavLink, Outlet } from 'react-router-dom';
import { useTheme } from '@shared/theme/ThemeProvider';

const navItems = [
  { to: '/quiz', label: '测评' },
  { to: '/learn', label: '学习' },
  { to: '/kanban', label: '看板' },
  { to: '/dashboard', label: '仪表盘' },
];

export function Layout() {
  const { theme, toggle } = useTheme();

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
          Reading Radar · MVP-5.1
        </div>
      </footer>
    </div>
  );
}