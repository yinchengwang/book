/**
 * ExportService 数据导出服务
 * 支持 JSON、Markdown、CSV 格式导出
 */
import { DataService } from './dataService'

// 导出格式枚举
export type ExportFormat = 'json' | 'markdown' | 'csv'

// 导出配置
export interface ExportConfig {
  format: ExportFormat
  includeKnowledge?: boolean
  includeReview?: boolean
  includeInterview?: boolean
  includeExcerpt?: boolean
  includeLearningPath?: boolean
  includeProject?: boolean
}

// ============= JSON 导出 =============

const exportToJSON = async (config: ExportConfig): Promise<string> => {
  const data: Record<string, any> = {
    exportTime: new Date().toISOString(),
    version: '1.0.0',
  }

  if (config.includeKnowledge !== false) {
    data.knowledge = DataService.knowledge.getAll()
  }
  if (config.includeReview !== false) {
    data.reviewStats = DataService.review.getStats()
    data.reviewHistory = DataService.review.getHistory()
  }
  if (config.includeInterview !== false) {
    data.interviews = DataService.interview.getAll()
  }
  if (config.includeExcerpt !== false) {
    data.excerpts = DataService.excerpt.getAll()
  }
  if (config.includeLearningPath !== false) {
    data.learningPaths = DataService.learningPath.getAll()
  }
  if (config.includeProject !== false) {
    data.projects = DataService.project.getAll()
  }

  return JSON.stringify(data, null, 2)
}

// ============= Markdown 导出 =============

const exportToMarkdown = async (config: ExportConfig): Promise<string> => {
  const lines: string[] = []

  // 标题
  lines.push('# Reading Radar 2.0 数据导出')
  lines.push(`\n导出时间：${new Date().toLocaleString('zh-CN')}`)
  lines.push('')

  // 知识库
  if (config.includeKnowledge !== false) {
    const knowledge = DataService.knowledge.getAll()
    lines.push('## 📚 知识库')
    lines.push('')
    lines.push(`共 ${knowledge.length} 个知识点`)
    lines.push('')

    // 按分类分组
    const grouped = knowledge.reduce((acc, item) => {
      acc[item.category] = acc[item.category] || []
      acc[item.category].push(item)
      return acc
    }, {} as Record<string, any[]>)

    for (const [category, items] of Object.entries(grouped)) {
      lines.push(`### ${category}`)
      lines.push('')
      lines.push('| 名称 | 掌握度 | 状态 |')
      lines.push('|------|--------|------|')
      for (const item of items) {
        const statusText = item.mastery >= 80 ? '✅ 已掌握' : item.mastery >= 50 ? '📖 学习中' : '❌ 待加强'
        lines.push(`| ${item.name} | ${item.mastery}% | ${statusText} |`)
      }
      lines.push('')
    }
  }

  // 复习统计
  if (config.includeReview !== false) {
    const stats = DataService.review.getStats()
    lines.push('## 📝 复习统计')
    lines.push('')
    lines.push(`| 指标 | 数值 |`)
    lines.push('|------|------|')
    lines.push(`| 总复习项 | ${stats.total} |`)
    lines.push(`| 已掌握 | ${stats.mastered} |`)
    lines.push(`| 学习中 | ${stats.learning} |`)
    lines.push(`| 薄弱点 | ${stats.weak} |`)
    lines.push('')
  }

  // 面试追踪
  if (config.includeInterview !== false) {
    const interviews = DataService.interview.getAll()
    lines.push('## 💼 面试追踪')
    lines.push('')
    lines.push(`共 ${interviews.length} 条记录`)
    lines.push('')
    lines.push('| 公司 | 岗位 | 状态 |')
    lines.push('|------|------|------|')
    for (const interview of interviews) {
      lines.push(`| ${interview.company} | ${interview.position} | ${interview.status} |`)
    }
    lines.push('')
  }

  // 摘抄
  if (config.includeExcerpt !== false) {
    const excerpts = DataService.excerpt.getAll()
    lines.push('## 📖 摘抄笔记')
    lines.push('')
    lines.push(`共 ${excerpts.length} 条摘抄`)
    lines.push('')
    for (const excerpt of excerpts) {
      lines.push(`### ${excerpt.bookTitle || '未知书籍'}`)
      lines.push('')
      lines.push(excerpt.content)
      if (excerpt.note) {
        lines.push('')
        lines.push('> ' + excerpt.note)
      }
      lines.push('')
      lines.push('---')
      lines.push('')
    }
  }

  // 学习路径
  if (config.includeLearningPath !== false) {
    const paths = DataService.learningPath.getAll()
    lines.push('## 🛤️ 学习路径')
    lines.push('')
    for (const path of paths) {
      lines.push(`### ${path.title}`)
      lines.push('')
      const completed = path.steps?.filter(s => s.status === 'completed').length || 0
      const total = path.steps?.length || 0
      lines.push(`进度：${completed}/${total} (${total > 0 ? Math.round(completed / total * 100) : 0}%)`)
      lines.push('')
    }
  }

  // 项目
  if (config.includeProject !== false) {
    const projects = DataService.project.getAll()
    lines.push('## 📊 项目')
    lines.push('')
    lines.push('| 项目名 | 状态 |')
    lines.push('|--------|------|')
    for (const project of projects) {
      lines.push(`| ${project.name} | ${project.status} |`)
    }
    lines.push('')
  }

  lines.push('\n---\n')
  lines.push('*由 Reading Radar 2.0 自动导出*')

  return lines.join('\n')
}

// ============= CSV 导出 =============

const exportToCSV = async (config: ExportConfig): Promise<string> => {
  const rows: string[][] = []

  // 知识库 CSV
  if (config.includeKnowledge !== false) {
    rows.push(['=== 知识库 ==='])
    rows.push(['名称', '分类', '掌握度', '状态'])

    const knowledge = DataService.knowledge.getAll()
    for (const item of knowledge) {
      const status = item.mastery >= 80 ? '已掌握' : item.mastery >= 50 ? '学习中' : '待加强'
      rows.push([item.name, item.category, String(item.mastery), status])
    }
    rows.push([])
  }

  // 面试追踪 CSV
  if (config.includeInterview !== false) {
    rows.push(['=== 面试追踪 ==='])
    rows.push(['公司', '岗位', '状态', '备注'])

    const interviews = DataService.interview.getAll()
    for (const interview of interviews) {
      rows.push([
        interview.company,
        interview.position,
        interview.status,
        interview.note || ''
      ])
    }
    rows.push([])
  }

  // 摘抄 CSV
  if (config.includeExcerpt !== false) {
    rows.push(['=== 摘抄 ==='])
    rows.push(['书籍', '内容', '标签', '笔记'])

    const excerpts = DataService.excerpt.getAll()
    for (const excerpt of excerpts) {
      rows.push([
        excerpt.bookTitle || '',
        excerpt.content || '',
        (excerpt.tags || []).join(';'),
        excerpt.note || ''
      ])
    }
    rows.push([])
  }

  // 转换为 CSV 字符串
  return rows.map(row =>
    row.map(cell =>
      `"${String(cell).replace(/"/g, '""')}"`
    ).join(',')
  ).join('\n')
}

// ============= 主导出函数 =============

export interface ExportResult {
  content: string
  filename: string
  mimeType: string
}

export const exportData = async (config: ExportConfig): Promise<ExportResult> => {
  const timestamp = new Date().toISOString().slice(0, 10).replace(/-/g, '')

  let content: string
  let filename: string
  let mimeType: string

  switch (config.format) {
    case 'json':
      content = await exportToJSON(config)
      filename = `reading-radar-export-${timestamp}.json`
      mimeType = 'application/json;charset=utf-8'
      break
    case 'markdown':
      content = await exportToMarkdown(config)
      filename = `reading-radar-export-${timestamp}.md`
      mimeType = 'text/markdown;charset=utf-8'
      break
    case 'csv':
      content = await exportToCSV(config)
      filename = `reading-radar-export-${timestamp}.csv`
      mimeType = 'text/csv;charset=utf-8'
      break
    default:
      throw new Error(`不支持的导出格式: ${config.format}`)
  }

  return { content, filename, mimeType }
}

// 下载文件
export const downloadFile = (result: ExportResult) => {
  // #ifdef H5
  const blob = new Blob([result.content], { type: result.mimeType })
  const url = URL.createObjectURL(blob)
  const link = document.createElement('a')
  link.href = url
  link.download = result.filename
  document.body.appendChild(link)
  link.click()
  document.body.removeChild(link)
  URL.revokeObjectURL(url)
  // #endif

  // #ifdef MP-WEIXIN
  wx.showToast({
    title: '导出功能在小程序端需手动复制',
    icon: 'none'
  })
  // #endif
}

// 一键导出
export const exportAll = async (format: ExportFormat = 'json') => {
  const result = await exportData({
    format,
    includeKnowledge: true,
    includeReview: true,
    includeInterview: true,
    includeExcerpt: true,
    includeLearningPath: true,
    includeProject: true,
  })
  downloadFile(result)
  return result
}

export default {
  exportData,
  exportAll,
  downloadFile,
}
