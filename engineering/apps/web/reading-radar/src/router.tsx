// src/router.tsx
import { createBrowserRouter, RouterProvider } from 'react-router-dom';
import { Layout } from '@/components/Layout';
import { Home } from '@/pages/Home';
import { Quiz } from '@/pages/Quiz';
import { Learn } from '@/pages/Learn';
import { Kanban } from '@/pages/Kanban';
import { Dashboard } from '@/pages/Dashboard';

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
    ],
  },
]);

export function Router() {
  return <RouterProvider router={router} />;
}