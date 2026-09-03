/**
 * H5 端 App 入口
 * 完整路由 + Layout + ErrorBoundary（每个页面独立包裹）
 */
import { Component, ErrorInfo, ReactNode, Suspense, lazy } from 'react'
import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom'
import Layout from './components/Layout/Layout'
import './styles/global.scss'

// 懒加载每个页面（独立 chunk，错误隔离）
const Dashboard = lazy(() => import('@/pages/index/index'))
const Quiz = lazy(() => import('@/pages/quiz/Quiz'))
const Review = lazy(() => import('@/pages/review/Review'))
const InterviewTracker = lazy(() => import('@/pages/interview_tracker/interview_tracker'))
const MockInterview = lazy(() => import('@/pages/mock_interview/mock_interview'))
const InterviewBank = lazy(() => import('@/pages/interview_bank/interview_bank'))
const KnowledgeGraph = lazy(() => import('@/pages/knowledge_graph/knowledge_graph'))
const LearningPath = lazy(() => import('@/pages/learning_path/learning_path'))
const Excerpt = lazy(() => import('@/pages/excerpt/Excerpt'))
const GapAnalysis = lazy(() => import('@/pages/gap_analysis/gap_analysis'))
const LearnDeepCategories = lazy(() => import('@/pages/learn_deep/LearnDeepCategories'))
const LearnDeepDetail = lazy(() => import('@/pages/learn_deep/LearnDeepDetail'))
const Radar = lazy(() => import('@/pages/radar/Radar'))
const FiveYearPlan = lazy(() => import('@/pages/five_year_plan/five_year_plan'))
const ProjectRoadmap = lazy(() => import('@/pages/project_roadmap/project_roadmap'))
const Settings = lazy(() => import('@/pages/settings/Settings'))
const NotFound = lazy(() => import('@/pages/not-found/NotFound'))

// 单页面 ErrorBoundary
class PageErrorBoundary extends Component<{ children: ReactNode; name?: string }, { hasError: boolean; error: Error | null }> {
  state = { hasError: false, error: null }

  static getDerivedStateFromError(error: Error) {
    return { hasError: true, error }
  }

  componentDidCatch(error: Error, info: ErrorInfo) {
    console.error(`[PageErrorBoundary${this.props.name ? ':' + this.props.name : ''}]`, error, info)
  }

  render() {
    if (this.state.hasError) {
      return (
        <div style={{ padding: 32, maxWidth: 900 }}>
          <div style={{ background: 'rgba(239,68,68,0.1)', border: '1px solid rgba(239,68,68,0.3)', borderRadius: 12, padding: 24 }}>
            <h2 style={{ color: '#fca5a5', marginBottom: 12, fontSize: 18 }}>
              ❌ {this.props.name || '页面'}加载失败
            </h2>
            <pre style={{
              background: 'rgba(0,0,0,0.3)',
              padding: 12,
              borderRadius: 8,
              fontSize: 12,
              color: '#fca5a5',
              whiteSpace: 'pre-wrap',
              wordBreak: 'break-all',
              maxHeight: 400,
              overflow: 'auto'
            }}>
              {this.state.error?.name}: {this.state.error?.message}
              {'\n\n'}
              {this.state.error?.stack}
            </pre>
            <button
              onClick={() => this.setState({ hasError: false, error: null })}
              style={{
                marginTop: 16,
                padding: '8px 16px',
                background: 'linear-gradient(135deg,#6366f1,#8b5cf6)',
                color: '#fff',
                borderRadius: 8,
                border: 'none',
                cursor: 'pointer',
                fontSize: 13
              }}
            >
              🔄 重试
            </button>
          </div>
        </div>
      )
    }
    return this.props.children
  }
}

// 页面加载占位
function PageLoading() {
  return (
    <div style={{ padding: 80, textAlign: 'center', color: '#94a3b8' }}>
      <div style={{
        width: 32, height: 32,
        border: '3px solid rgba(255,255,255,0.1)',
        borderTopColor: '#818cf8',
        borderRadius: '50%',
        animation: 'spin 0.8s linear infinite',
        margin: '0 auto 16px'
      }} />
      加载中...
      <style>{`@keyframes spin { to { transform: rotate(360deg); } }`}</style>
    </div>
  )
}

// 包装每个页面
function wrap(name: string, Comp: React.LazyExoticComponent<any>) {
  return (
    <PageErrorBoundary name={name}>
      <Suspense fallback={<PageLoading />}>
        <Comp />
      </Suspense>
    </PageErrorBoundary>
  )
}

export default function App() {
  return (
    <BrowserRouter>
      <Layout>
        <Routes>
          <Route path="/" element={wrap('仪表盘', Dashboard)} />
          <Route path="/quiz" element={wrap('题库练习', Quiz)} />
          <Route path="/review" element={wrap('复习计划', Review)} />
          <Route path="/interview_tracker" element={wrap('面试追踪', InterviewTracker)} />
          <Route path="/mock_interview" element={wrap('模拟面试', MockInterview)} />
          <Route path="/interview_bank" element={wrap('面试题库', InterviewBank)} />
          <Route path="/knowledge_graph" element={wrap('知识图谱', KnowledgeGraph)} />
          <Route path="/learning_path" element={wrap('学习路径', LearningPath)} />
          <Route path="/excerpt" element={wrap('摘抄管理', Excerpt)} />
          <Route path="/gap_analysis" element={wrap('差距分析', GapAnalysis)} />
          <Route path="/learn_deep" element={wrap('图解系列', LearnDeepCategories)} />
          <Route path="/learn_deep/:category" element={wrap('图解系列详情', LearnDeepDetail)} />
          <Route path="/radar" element={wrap('技术雷达', Radar)} />
          <Route path="/five_year_plan" element={wrap('五年计划', FiveYearPlan)} />
          <Route path="/project_roadmap" element={wrap('项目路线', ProjectRoadmap)} />
          <Route path="/settings" element={wrap('设置', Settings)} />
          <Route path="/pages/*" element={<Navigate to="/" replace />} />
          <Route path="*" element={wrap('404', NotFound)} />
        </Routes>
      </Layout>
    </BrowserRouter>
  )
}
