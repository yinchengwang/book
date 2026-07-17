/**
 * 图解系列文章阅读器组件
 */
import { View, Text, ScrollView } from '@tarojs/components'
import { LearnDeepSummary, CategoryId, CATEGORIES } from '@/data/learn_deep_index'
import './learn_deep.scss'

// #ifdef H5
import { MarkdownRenderer } from '@/components/markdown/MarkdownRenderer'
// #endif

interface ArticleReaderProps {
  slug: string
  article?: LearnDeepSummary
  content?: string
  allArticles: LearnDeepSummary[]
  onBack: () => void
  onNavigate: (slug: string) => void
}

export const ArticleReader = ({
  slug,
  article,
  content,
  allArticles,
  onBack,
  onNavigate,
}: ArticleReaderProps) => {
  if (!article || !content) {
    return (
      <View className="article-reader">
        <View className="reader-error">
          <Text className="error-text">文章未找到</Text>
          <Text className="back-btn" onClick={onBack}>← 返回</Text>
        </View>
      </View>
    )
  }

  // 获取同分类文章
  const sameCategory = allArticles.filter(a => a.category === article.category)
  const currentIdx = sameCategory.findIndex(a => a.slug === slug)
  const prev = currentIdx > 0 ? sameCategory[currentIdx - 1] : null
  const next = currentIdx < sameCategory.length - 1 ? sameCategory[currentIdx + 1] : null

  // 获取分类名称
  const categoryName = CATEGORIES.find(c => c.id === article.category)?.name || article.category

  return (
    <View className="article-reader">
      <ScrollView scrollY className="reader-scroll">
        {/* 面包屑导航 */}
        <View className="reader-breadcrumb">
          <Text className="breadcrumb-link" onClick={onBack}>📖 图解系列</Text>
          <Text className="breadcrumb-sep">/</Text>
          <Text className="breadcrumb-cat">{categoryName}</Text>
        </View>

        {/* 文章元信息 */}
        <View className="reader-header">
          <View className="reader-meta">
            <Text className="badge category">{categoryName}</Text>
            <Text className="meta-length">📖 {Math.ceil(article.length / 500)} 分钟阅读</Text>
          </View>
          <Text className="reader-title">{article.title}</Text>
          {article.excerpt && (
            <Text className="reader-excerpt">{article.excerpt}</Text>
          )}
        </View>

        {/* 文章内容 */}
        <View className="reader-content">
          {/* H5 端使用 react-markdown */}
          {/* #ifdef H5 */}
          <MarkdownRenderer content={content} />
          {/* #endif */}

          {/* 小程序端使用简单渲染 */}
          {/* #ifndef H5 */}
          <View className="markdown-simple">
            {content.split('\n').map((line, i) => (
              <Text key={i} className={`md-line ${line.startsWith('# ') ? 'md-h1' : line.startsWith('## ') ? 'md-h2' : ''}`}>
                {line}
              </Text>
            ))}
          </View>
          {/* #endif */}
        </View>

        {/* 上/下篇导航 */}
        <View className="reader-footer">
          {prev ? (
            <View className="nav-card nav-prev" onClick={() => onNavigate(prev.slug)}>
              <Text className="nav-label">← 上一篇</Text>
              <Text className="nav-title">{prev.title}</Text>
            </View>
          ) : (
            <View className="nav-card nav-prev nav-disabled">
              <Text className="nav-label">← 上一篇</Text>
              <Text className="nav-title">已经是第一篇了</Text>
            </View>
          )}
          {next ? (
            <View className="nav-card nav-next" onClick={() => onNavigate(next.slug)}>
              <Text className="nav-label">下一篇 →</Text>
              <Text className="nav-title">{next.title}</Text>
            </View>
          ) : (
            <View className="nav-card nav-next nav-disabled">
              <Text className="nav-label">下一篇 →</Text>
              <Text className="nav-title">已经是最后一篇了</Text>
            </View>
          )}
        </View>
      </ScrollView>
    </View>
  )
}