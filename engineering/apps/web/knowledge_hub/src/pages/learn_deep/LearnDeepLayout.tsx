/**
 * 图解系列布局组件
 *
 * H5 端：双栏布局（左侧目录 + 右侧内容）
 * 小程序端：保持简单列表模式
 */
import { useState } from 'react'
import { View, Text, Input, ScrollView } from '@tarojs/components'
import { CATEGORIES, getLearnDeepByCategory, CategoryId, LearnDeepSummary } from '@/data/learn_deep_index'
import { CategorySidebar } from './CategorySidebar'
import { ArticleReader } from './ArticleReader'
import { SearchBar } from './SearchBar'
import './learn_deep.scss'

interface LearnDeepLayoutProps {
  articleList: LearnDeepSummary[]
  articleContent: Record<string, string>
}

export const LearnDeepLayout = ({ articleList, articleContent }: LearnDeepLayoutProps) => {
  // 当前选中的分类
  const [selectedCategory, setSelectedCategory] = useState<CategoryId | null>(null)
  // 当前选中的文章 slug
  const [selectedSlug, setSelectedSlug] = useState<string | null>(null)
  // 搜索关键词
  const [searchQuery, setSearchQuery] = useState('')
  // 折叠的分类
  const [collapsedCategories, setCollapsedCategories] = useState<Set<CategoryId>>(
    new Set(CATEGORIES.slice(1).map(c => c.id))
  )

  // 过滤后的文章列表
  const filteredList = searchQuery.trim()
    ? articleList.filter(a =>
        a.title.toLowerCase().includes(searchQuery.toLowerCase()) ||
        a.excerpt.toLowerCase().includes(searchQuery.toLowerCase())
      )
    : selectedCategory
      ? articleList.filter(a => a.category === selectedCategory)
      : articleList

  // 切换分类折叠状态
  const toggleCategory = (catId: CategoryId) => {
    setCollapsedCategories(prev => {
      const next = new Set(prev)
      if (next.has(catId)) {
        next.delete(catId)
      } else {
        next.add(catId)
      }
      return next
    })
  }

  // 选择文章
  const selectArticle = (slug: string) => {
    setSelectedSlug(slug)
  }

  // 返回列表
  const backToList = () => {
    setSelectedSlug(null)
  }

  // 选择分类
  const selectCategory = (catId: CategoryId) => {
    setSelectedCategory(catId)
    setSelectedSlug(null)
  }

  return (
    <View className="learn-deep-layout">
      {/* 顶部栏 */}
      <View className="layout-header">
        <Text className="header-title">📖 图解系列</Text>
        <View className="header-search">
          <SearchBar value={searchQuery} onChange={setSearchQuery} />
        </View>
      </View>

      {/* 双栏主体 */}
      <View className="layout-body">
        {/* 左侧目录栏 */}
        <CategorySidebar
          categories={CATEGORIES}
          categoryArticles={getLearnDeepByCategory()}
          selectedSlug={selectedSlug}
          selectedCategory={selectedCategory}
          collapsedCategories={collapsedCategories}
          onToggleCategory={toggleCategory}
          onSelectArticle={selectArticle}
          onSelectCategory={selectCategory}
        />

        {/* 右侧内容区 */}
        <View className="layout-content">
          {selectedSlug ? (
            <ArticleReader
              slug={selectedSlug}
              article={articleList.find(a => a.slug === selectedSlug)}
              content={articleContent[selectedSlug]}
              allArticles={articleList}
              onBack={backToList}
              onNavigate={selectArticle}
            />
          ) : (
            <View className="content-placeholder">
              <Text className="placeholder-icon">📚</Text>
              <Text className="placeholder-text">从左侧选择文章阅读</Text>
            </View>
          )}
        </View>
      </View>
    </View>
  )
}