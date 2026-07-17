/**
 * 图解系列数据迁移
 *
 * 来源：project/reading-radar/data/app/learn-deep-bundle.js
 * 输出：src/data/learn-deep.ts
 *
 * 策略：保留完整内容，但用单独文件 + 异步加载
 */

const fs = require('fs')
const path = require('path')
const vm = require('vm')

const SOURCE_FILE = path.join(__dirname, '..', '..', '..', 'project', 'reading-radar', 'data', 'app', 'learn-deep-bundle.js')
const OUTPUT_DIR = path.join(__dirname, '..', '..', 'src', 'data')

function extractLearnDeep() {
  const content = fs.readFileSync(SOURCE_FILE, 'utf-8')
  const sandbox = {
    window: {},
    console
  }
  vm.createContext(sandbox)

  try {
    vm.runInContext(content, sandbox, { filename: SOURCE_FILE })
    return sandbox.window.LEARN_DEEP_CONTENT || sandbox.LEARN_DEEP_CONTENT || {}
  } catch (e) {
    console.error('解析失败:', e.message)
    return {}
  }
}

function main() {
  console.log('开始迁移 learn-deep 图解系列...')
  console.log('源文件:', SOURCE_FILE)

  if (!fs.existsSync(OUTPUT_DIR)) {
    fs.mkdirSync(OUTPUT_DIR, { recursive: true })
  }

  const data = extractLearnDeep()
  const entries = Object.entries(data)
  console.log(`共 ${entries.length} 篇图解文章`)

  // 提取每个图解的标题和元数据
  const summaries = entries.map(([slug, content]) => {
    // 提取标题（第一行 # 开头）
    const titleMatch = content.match(/^#\s+(.+)$/m)
    const title = titleMatch ? titleMatch[1].trim() : slug

    // 提取前 300 字符作为摘要
    const cleanContent = content.replace(/^#\s+.+$/m, '').trim()
    const excerpt = cleanContent.slice(0, 300).replace(/\n+/g, ' ').trim() + '...'

    // 提取分类（从 slug 推断）
    let category = '综合'
    if (slug.startsWith('c-') && !slug.startsWith('cpp-')) category = 'C'
    else if (slug.startsWith('cpp-')) category = 'C++'
    else if (slug.startsWith('py-')) category = 'Python'
    else if (slug.startsWith('db-') || slug.startsWith('mysql-') || slug.startsWith('redis-')) category = '数据库'
    else if (slug.startsWith('os-') || slug.startsWith('system-')) category = '操作系统'
    else if (slug.startsWith('net-') || slug.startsWith('network-')) category = '网络'
    else if (slug.startsWith('algo-') || slug.startsWith('ds-')) category = '算法'

    return {
      slug,
      title,
      category,
      excerpt,
      length: content.length,
      content: undefined  // 不打包到摘要，按需加载
    }
  })

  // 写主索引文件
  const indexContent = `// 自动生成 - 图解系列数据
// 来源：旧版 reading-radar learn-deep-bundle.js
// 共 ${summaries.length} 篇图解文章
// 生成时间：${new Date().toISOString()}

export interface LearnDeepSummary {
  slug: string
  title: string
  category: string
  excerpt: string
  length: number
}

export const learnDeepList: LearnDeepSummary[] = ${JSON.stringify(summaries, null, 2)}

// 按分类分组
export const learnDeepByCategory: Record<string, LearnDeepSummary[]> = {}
for (const item of learnDeepList) {
  if (!learnDeepByCategory[item.category]) learnDeepByCategory[item.category] = []
  learnDeepByCategory[item.category].push(item)
}

export const learnDeepStats = {
  total: ${summaries.length},
  categories: ${[...new Set(summaries.map(s => s.category))].length},
  byCategory: {
${[...new Set(summaries.map(s => s.category))].map(c => `    ${JSON.stringify(c)}: ${summaries.filter(s => s.category === c).length}`).join(',\n')}
  }
}
`
  fs.writeFileSync(path.join(OUTPUT_DIR, 'learn-deep-index.ts'), indexContent, 'utf-8')
  console.log(`生成: src/data/learn-deep-index.ts`)

  // 写完整内容（单独文件，按需加载）
  const contentMap = entries.map(([slug, content]) => `  ${JSON.stringify(slug)}: ${JSON.stringify(content)}`).join(',\n')
  const contentContent = `// 自动生成 - 图解系列完整内容
// 来源：旧版 reading-radar learn-deep-bundle.js
// 共 ${entries.length} 篇
// 生成时间：${new Date().toISOString()}

export const learnDeepContent: Record<string, string> = {
${contentMap}
}
`
  fs.writeFileSync(path.join(OUTPUT_DIR, 'learn-deep-content.ts'), contentContent, 'utf-8')
  console.log(`生成: src/data/learn-deep-content.ts`)

  console.log('\n分类统计:')
  for (const [cat, count] of Object.entries({
    ...summaries.reduce((acc, s) => { acc[s.category] = (acc[s.category] || 0) + 1; return acc }, {})
  })) {
    console.log(`  ${cat}: ${count} 篇`)
  }

  // 总大小
  const totalSize = fs.statSync(path.join(OUTPUT_DIR, 'learn-deep-content.ts')).size
  console.log(`\n内容文件大小: ${(totalSize / 1024 / 1024).toFixed(2)} MB`)
}

main()