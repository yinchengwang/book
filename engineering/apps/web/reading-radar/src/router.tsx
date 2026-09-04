// src/router.tsx
import { createBrowserRouter, RouterProvider } from 'react-router-dom';
import { Layout } from '@/components/Layout';
import { Home } from '@/pages/Home';
import { Quiz } from '@/pages/Quiz';
import { Learn } from '@/pages/Learn';
import { Kanban } from '@/pages/Kanban';
import { Dashboard } from '@/pages/Dashboard';
import { NotFound } from './pages/NotFound';

const router = createBrowserRouter([
  {
    path: '/',
    element: <Layout />,
    children: [
      { index: true, element: <Home /> },
      { path: 'quiz', element: <Quiz /> },
      { path: 'learn', element: <Learn /> },
      { path: 'learn/:cat/:item', element: <Learn /> },
      { path: 'kanban', element: <Kanban /> },
      { path: 'dashboard', element: <Dashboard /> },
      { path: '*', element: <NotFound /> }
    ],
  },
]);

export function Router() {
  return <RouterProvider router={router} />;
}