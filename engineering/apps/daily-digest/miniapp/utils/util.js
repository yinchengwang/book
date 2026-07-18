const formatTime = (dateStr) => {
  if (!dateStr) return ''
  const d = new Date(dateStr)
  const now = new Date()
  const diff = now.getTime() - d.getTime()
  const days = Math.floor(diff / 86400000)
  if (days === 0) return '今天'
  if (days === 1) return '昨天'
  if (days < 7) return `${days} 天前`
  return `${d.getMonth() + 1}/${d.getDate()}`
}

const truncate = (text, len) => {
  if (!text) return ''
  return text.length > len ? text.slice(0, len) + '...' : text
}

const SOURCE_LABELS = {
  arxiv: 'arXiv',
  semantic_scholar: 'Semantic Scholar',
  dblp: 'DBLP',
  huggingface: 'HuggingFace',
  blog: '博客',
  github: 'GitHub',
}

const CATEGORY_LABELS = {
  db: '数据库',
  ai: 'AI',
  ml: 'ML',
  infra: '基础设施',
  sys: '系统',
  other: '其他',
}

module.exports = { formatTime, truncate, SOURCE_LABELS, CATEGORY_LABELS }
