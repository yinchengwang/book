/**
 * 图解系列分类侧边栏组件
 */
import { View, Text, ScrollView } from '@tarojs/components'
import { CategoryMeta, LearnDeepSummary, CategoryId } from '@/data/learn_deep_index'
import './learn_deep.scss'

interface CategorySidebarProps {
  categories: CategoryMeta[]
  categoryArticles: Record<CategoryId, LearnDeepSummary[]>
  selectedSlug: string | null
  selectedCategory: CategoryId | null
  collapsedCategories: Set<CategoryId>
  onToggleCategory: (catId: CategoryId) => void
  onSelectArticle: (slug: string) => void
  onSelectCategory: (catId: CategoryId) => void
}

export const CategorySidebar = ({
  categories,
  categoryArticles,
  selectedSlug,
  selectedCategory,
  collapsedCategories,
  onToggleCategory,
  onSelectArticle,
  onSelectCategory,
}: CategorySidebarProps) => {
  // 获取当前选中文章所在的分类
  const getArticleCategory = () => {
    if (!selectedSlug) return null
    for (const cat of categories) {
      if (categoryArticles[cat.id]?.some(a => a.slug === selectedSlug)) {
        return cat.id
      }
    }
    return null
  }

  const activeCategory = selectedCategory || getArticleCategory()

  return (
    <View className="category-sidebar">
      <View className="sidebar-header">
        <Text className="sidebar-title">📖 图解系列</Text>
      </View>

      <ScrollView scrollY className="sidebar-scroll">
        {categories.map(cat => {
          const articles = categoryArticles[cat.id] || []
          const isCollapsed = collapsedCategories.has(cat.id)
          const isActive = activeCategory === cat.id

          return (
            <View key={cat.id} className={`category-group ${isActive ? 'active' : ''}`}>
              {/* 分类标题 */}
              <View
                className="category-header"
                onClick={() => {
                  onSelectCategory(cat.id)
                  if (isCollapsed) {
                    onToggleCategory(cat.id)
                  }
                }}
              >
                <Text className={`collapse-arrow ${isCollapsed ? 'collapsed' : ''}`}>
                  {isCollapsed ? '▶' : '▼'}
                </Text>
                <Text className="category-name">{cat.name}</Text>
                <Text className="category-count">({articles.length})</Text>
              </View>

              {/* 文章列表 */}
              {!isCollapsed && articles.length > 0 && (
                <View className="article-list">
                  {articles.map(article => (
                    <View
                      key={article.slug}
                      className={`article-item ${selectedSlug === article.slug ? 'selected' : ''}`}
                      onClick={() => onSelectArticle(article.slug)}
                    >
                      <Text className="article-title">{article.title}</Text>
                    </View>
                  ))}
                </View>
              )}
            </View>
          )
        })}
      </ScrollView>
    </View>
  )
}