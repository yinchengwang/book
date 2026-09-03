/**
 * 摘抄数据迁移脚本
 *
 * 数据源：c:\code\book\project\reading-radar\data\excerpt\**\*.md
 * 输出：c:\code\book\reading-radar-2\src\data\excerpt\initialExcerpts.ts
 *
 * 老项目格式：
 * ---
 * book: "c++新经典"
 * date: "2025"
 * tags: [C++]
 * ---
 *
 * ## 摘抄 1
 * 内容...
 *
 * ## 摘抄 2
 * 内容...
 */
const fs = require('fs')
const path = require('path')

// 路径常量
const PROJECT_ROOT = path.resolve(__dirname, '..', '..', '..')
const OLD_EXCERPT_DIR = path.join(PROJECT_ROOT, 'project', 'reading-radar', 'data', 'excerpt')
const META_FILE = path.join(PROJECT_ROOT, 'project', 'reading-radar', 'data', 'excerpt-meta.json')
const OUTPUT_DIR = path.resolve(__dirname, '..', '..', 'src', 'data', 'excerpt')
const OUTPUT_FILE = path.join(OUTPUT_DIR, 'initialExcerpts.ts')

// ===== 工具函数 =====

/**
 * 极简 YAML 解析（仅支持 key: value / tags: [a, b] / 嵌套 list）
 */
function parseFrontmatter(raw) {
  const match = raw.match(/^---\s*\n([\s\S]*?)\n---\s*\n([\s\S]*)$/)
  if (!match) return { meta: null, body: raw }

  const yamlText = match[1]
  const body = match[2]
  const meta = {}

  // 逐行解析
  const lines = yamlText.split('\n')
  let i = 0
  while (i < lines.length) {
    const line = lines[i]
    const kvMatch = line.match(/^([a-zA-Z_][\w]*)\s*:\s*(.*)$/)
    if (!kvMatch) { i++; continue }

    const key = kvMatch[1]
    let value = kvMatch[2].trim()

    // 处理数组 [a, b, "c"]
    if (value.startsWith('[') && value.endsWith(']')) {
      value = value.slice(1, -1)
        .split(',')
        .map(v => v.trim().replace(/^["']|["']$/g, ''))
        .filter(Boolean)
    }
    // 处理字符串 "..."
    else if ((value.startsWith('"') && value.endsWith('"')) ||
             (value.startsWith("'") && value.endsWith("'"))) {
      value = value.slice(1, -1)
    }
    // 处理空值
    else if (value === '' || value === '~' || value === 'null') {
      value = null
    }
    // 处理布尔
    else if (value === 'true') value = true
    else if (value === 'false') value = false

    meta[key] = value
    i++
  }

  return { meta, body }
}

/**
 * 按 "## 摘抄 N" 或 "## N" 切分正文
 * 单段无分隔则整段作为 1 条
 */
function splitSections(body) {
  // 标准分隔：## 摘抄 1、## 摘抄 2、...
  const regex = /^##\s*(?:摘抄\s*)?\d+\s*$/m
  const matches = [...body.matchAll(new RegExp(regex.source, 'gm'))]

  if (matches.length === 0) {
    // 无分隔符，整段作为 1 条
    const content = body.trim()
    if (content) return [content]
    return []
  }

  const sections = []
  for (let i = 0; i < matches.length; i++) {
    const start = matches[i].index + matches[i][0].length
    const end = i + 1 < matches.length ? matches[i + 1].index : body.length
    const content = body.slice(start, end).trim()
    if (content) sections.push(content)
  }
  return sections
}

/**
 * 把字符串转为稳定 ID
 */
function makeId(book, idx) {
  return `${book.replace(/[^\w一-龥]/g, '_')}-${idx}`
}

/**
 * 解析单个 MD 文件
 */
function parseMD(filePath) {
  const raw = fs.readFileSync(filePath, 'utf8')
  const { meta, body } = parseFrontmatter(raw)

  if (!meta || !meta.book) {
    console.warn(`  ⚠️  ${path.basename(filePath)}: 无 book 字段，跳过`)
    return []
  }

  const sections = splitSections(body)
  const items = []

  for (let i = 0; i < sections.length; i++) {
    const content = sections[i]
    if (!content || content.length < 2) continue

    items.push({
      id: makeId(meta.book, i + 1),
      content,
      book: meta.book,
      tags: Array.isArray(meta.tags) ? meta.tags : [],
      date: meta.date || '',
      favorite: false,
      createdAt: `${meta.date || '2025'}-01-01T00:00:00Z`,
      // 新字段：来源标记
      source: path.basename(filePath, '.md'),
      // 阅读状态（从 excerpt-meta.json 取）
      status: undefined
    })
  }

  return items
}

/**
 * 加载 excerpt-meta.json
 */
function loadMeta() {
  try {
    return JSON.parse(fs.readFileSync(META_FILE, 'utf8'))
  } catch {
    return { statuses: {} }
  }
}

/**
 * 扫描所有摘抄 MD 文件
 */
function scanAll() {
  console.log(`📂 扫描目录：${OLD_EXCERPT_DIR}`)
  const all = []

  if (!fs.existsSync(OLD_EXCERPT_DIR)) {
    console.error(`❌ 目录不存在：${OLD_EXCERPT_DIR}`)
    return all
  }

  const years = fs.readdirSync(OLD_EXCERPT_DIR)
    .filter(d => {
      const full = path.join(OLD_EXCERPT_DIR, d)
      return fs.statSync(full).isDirectory()
    })

  for (const year of years) {
    const yearDir = path.join(OLD_EXCERPT_DIR, year)
    const files = fs.readdirSync(yearDir).filter(f => f.endsWith('.md'))
    console.log(`  📁 ${year}/ (${files.length} 文件)`)
    for (const f of files) {
      const fp = path.join(yearDir, f)
      const items = parseMD(fp)
      console.log(`    ✅ ${f}: ${items.length} 条摘抄`)
      all.push(...items)
    }
  }

  return all
}

/**
 * 生成 TS 文件内容
 */
function generateTS(data) {
  const lines = [
    `/**`,
    ` * 摘抄初始数据 - 自动生成于 migrate-excerpt.cjs`,
    ` * 数据源：project/reading-radar/data/excerpt/[year]/[book].md`,
    ` *`,
    ` * 字段：id / content / book / tags / date / favorite / createdAt / source / status`,
    ` */`,
    ``,
    `import { Excerpt } from '@/stores/excerptStore'`,
    ``,
    `export const initialExcerpts: Excerpt[] = [`
  ]

  for (const item of data) {
    const json = JSON.stringify(item, null, 2)
    const indented = json.split('\n').map((l, i) => i === 0 ? l : '  ' + l).join('\n')
    lines.push(`  ${indented},`)
  }

  lines.push(`]`)
  return lines.join('\n')
}

function main() {
  console.log('=== 摘抄数据迁移 ===\n')

  const meta = loadMeta()
  const statuses = meta.statuses || {}

  const excerpts = scanAll()

  // 应用阅读状态
  for (const e of excerpts) {
    if (statuses[e.book]) {
      e.status = statuses[e.book]
    }
  }

  // 按书聚合统计
  const byBook = {}
  for (const e of excerpts) {
    byBook[e.book] = (byBook[e.book] || 0) + 1
  }

  // 去重（按 id）
  const seen = new Set()
  const deduped = []
  for (const e of excerpts) {
    if (seen.has(e.id)) continue
    seen.add(e.id)
    deduped.push(e)
  }

  console.log(`\n=== 统计 ===`)
  console.log(`原始条目：${excerpts.length}`)
  console.log(`去重后：${deduped.length}`)
  console.log(`覆盖书数：${Object.keys(byBook).length} 本`)
  console.log(`\n书 → 摘抄数：`)
  const sortedBooks = Object.entries(byBook).sort((a, b) => b[1] - a[1])
  for (const [book, count] of sortedBooks) {
    console.log(`  ${book}: ${count}`)
  }

  // 写文件
  console.log(`\n=== 写入文件 ===`)
  fs.mkdirSync(OUTPUT_DIR, { recursive: true })
  fs.writeFileSync(OUTPUT_FILE, generateTS(deduped), 'utf8')
  console.log(`  📝 ${OUTPUT_FILE}`)
  console.log(`  📦 ${(fs.statSync(OUTPUT_FILE).size / 1024).toFixed(1)} KB`)

  console.log(`\n✅ 迁移完成！共 ${deduped.length} 条摘抄。`)
}

main()