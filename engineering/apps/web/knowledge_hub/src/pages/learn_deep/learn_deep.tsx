/**
 * 图解系列页面
 *
 * 展示 396 篇图解文章
 * 支持按分类筛选、搜索、详情查看
 *
 * H5 端：双栏布局（左侧目录 + 右侧内容）
 * 小程序端：保持简单列表模式
 */
import { LearnDeepLayout } from './LearnDeepLayout'
import { learnDeepList } from '@/data/learn_deep_index'
import { learnDeepContent } from '@/data/learn_deep_content'
import './learn_deep.scss'

// #ifdef H5
import { MarkdownRenderer } from '@/components/markdown/MarkdownRenderer'
// #endif

// 导出 MarkdownRenderer 供小程序端使用
export { MarkdownRenderer }

const LearnDeep = () => {
  return (
    <LearnDeepLayout
      articleList={learnDeepList}
      articleContent={learnDeepContent}
    />
  )
}

export default LearnDeep
