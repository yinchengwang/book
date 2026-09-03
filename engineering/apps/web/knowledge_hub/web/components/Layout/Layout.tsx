/**
 * Layout - 整体布局容器
 * Sidebar + TopBar + Content Area
 */
import { ReactNode, Suspense, useEffect } from 'react'
import Sidebar from './Sidebar'
import TopBar from './TopBar'
import { useThemeEffect } from '@/hooks/useThemeEffect'
import './Layout.scss'

interface LayoutProps {
  children: ReactNode
}

function PageLoading() {
  return (
    <div className="page-loading">
      <div className="spinner"></div>
      <p>加载中...</p>
    </div>
  )
}

export default function Layout({ children }: LayoutProps) {
  // 应用主题到 document.documentElement.dataset.theme
  useThemeEffect()

  // 清除 splash（首次加载 + 客户端路由切换都会触发）
  useEffect(() => {
    const splash = document.getElementById('splash')
    if (splash) {
      splash.style.display = 'none'
      setTimeout(() => splash.remove(), 100)
    }
  }, [])

  return (
    <>
      <Sidebar />
      <main className="main-content">
        <TopBar />
        <div className="page-container">
          <Suspense fallback={<PageLoading />}>
            {children}
          </Suspense>
        </div>
      </main>
    </>
  )
}
