/**
 * 面试题库数据迁移脚本
 *
 * 数据源：
 * 1. practice/*.js (25 类 × 1149 题 - LeetCode 题列表)
 * 2. learning/interview/*.md (~110 题 - 真实面试 Q&A)
 *
 * 输出：src/data/interview-bank/{algo,ccpp,database,vdb,misc}-bank.ts
 */
const fs = require('fs')
const path = require('path')
const vm = require('vm')

const PROJECT_ROOT = path.resolve(__dirname, '..', '..', '..')
const PRACTICE_DIR = path.join(PROJECT_ROOT, 'project', 'reading-radar', 'data', 'app', 'practice')
const INTERVIEW_DIR = path.join(PROJECT_ROOT, 'Interview')
const OUTPUT_DIR = path.resolve(__dirname, '..', '..', 'src', 'data', 'interview-bank')

// 输出目录
fs.mkdirSync(OUTPUT_DIR, { recursive: true })

// ==================== 算法题迁移 ====================

function migrateAlgoBank() {
  console.log('=== 迁移算法题（practice/*.js）===')
  const algoBank = []
  const algoMeta = []

  const files = fs.readdirSync(PRACTICE_DIR)
    .filter(f => f.endsWith('.js') && f !== '_index.js')
    .sort()

  for (const f of files) {
    const fp = path.join(PRACTICE_DIR, f)
    const code = fs.readFileSync(fp, 'utf8')
    const sandbox = { window: {} }
    vm.createContext(sandbox)
    try {
      vm.runInContext(code, sandbox)
    } catch (e) {
      console.error(`  ❌ ${f}: ${e.message}`)
      continue
    }
    const C = sandbox.window.PRACTICE_CATEGORIES
    if (!C || C.length === 0) {
      console.error(`  ❌ ${f}: 未找到 PRACTICE_CATEGORIES`)
      continue
    }
    for (const cat of C) {
      algoMeta.push({
        id: cat.id,
        title: cat.title,
        icon: cat.icon || '📘',
        desc: cat.desc || '',
        phase: cat.phase || 0,
        order: cat.order || 0,
        count: cat.items.length
      })
      for (const item of cat.items) {
        algoBank.push({
          id: `${cat.id}-${item.problemId}`,
          title: item.title,
          problemId: item.problemId,
          difficulty: item.difficulty || 'medium',
          tags: item.tags || [],
          url: item.url || '',
          platform: item.platform || 'leetcode',
          category: cat.id,
          categoryTitle: cat.title
        })
      }
      console.log(`  ✅ ${cat.id} (${cat.title}): ${cat.items.length} 题`)
    }
  }
  return { algoBank, algoMeta }
}

// ==================== 真实面试题迁移 ====================

function parseInterviewMD(filePath, source) {
  const md = fs.readFileSync(filePath, 'utf8')
  // 按 ## 切分（保留 ## 前缀）
  const sections = md.split(/^## /m).slice(1)
  const questions = []
  for (const sec of sections) {
    const lines = sec.split('\n')
    const title = lines[0].trim()
    // 清洗标题：去掉前导数字/中文数字/标点
    const cleanTitle = title
      .replace(/^[一二三四五六七八九十百千]+[、.\s]+/, '')
      .replace(/^\d+[、.\s]+/, '')
      .replace(/^Q[:：]\s*/i, '')
      .trim()
    if (!cleanTitle || cleanTitle.length < 2) continue
    const answer = lines.slice(1).join('\n').trim()
    if (answer.length < 10) continue  // 跳过空内容

    // 推断难度
    let difficulty = 'medium'
    if (/简单|入门|基础|easy|基础题/i.test(cleanTitle + ' ' + answer.slice(0, 200))) difficulty = 'easy'
    else if (/困难|进阶|高级|hard|专家/i.test(cleanTitle + ' ' + answer.slice(0, 200))) difficulty = 'hard'

    // 哈希 ID
    const id = `${source}-${Buffer.from(cleanTitle).toString('base64').slice(0, 12).replace(/[/+=]/g, '')}`
    questions.push({
      id,
      title: cleanTitle,
      answer,
      source,
      difficulty
    })
  }
  return questions
}

function migrateInterviewBank() {
  console.log('\n=== 迁移真实面试题（learning/interview/*.md）===')
  const interviewBank = []
  const interviewMeta = []

  const sources = [
    { source: 'ccpp', file: 'c_cpp/questions.md', title: 'C/C++ 基础', icon: '💻' },
    { source: 'ccpp-extra', file: 'c_cpp/leftover_problem.md', title: 'C/C++ 补充', icon: '💡' },
    { source: 'database', file: 'database/questions.md', title: '数据库基础', icon: '🗄️' },
    { source: 'database-interview', file: 'database/database-interview-questions.md', title: '数据库面试', icon: '📊' },
    { source: 'database-project', file: 'database/database-project-experience-questions.md', title: '数据库项目经验', icon: '🛠️' },
    { source: 'vdb', file: 'VDB/vdb-interview-questions.md', title: '向量数据库', icon: '🔢' },
    { source: 'vdb-extra', file: 'VDB/vdb.md', title: 'VDB 专题', icon: '📐' }
  ]

  for (const s of sources) {
    const fp = path.join(INTERVIEW_DIR, s.file)
    if (!fs.existsSync(fp)) {
      console.log(`  ⚠️  ${s.file} 不存在`)
      continue
    }
    const qs = parseInterviewMD(fp, s.source)
    if (qs.length === 0) {
      console.log(`  ⚠️  ${s.file} 无有效问题`)
      continue
    }
    interviewMeta.push({
      id: s.source,
      title: s.title,
      icon: s.icon,
      count: qs.length
    })
    interviewBank.push(...qs)
    console.log(`  ✅ ${s.title}: ${qs.length} 题`)
  }

  return { interviewBank, interviewMeta }
}

// ==================== 写入 TS 文件 ====================

function writeTSFile(filename, content) {
  const fp = path.join(OUTPUT_DIR, filename)
  fs.writeFileSync(fp, content, 'utf8')
  console.log(`  📝 ${filename} (${(content.length / 1024).toFixed(1)} KB)`)
}

function generateTSContent(varName, typeName, data, header) {
  // 手工序列化，避免引入 JSON.stringify 缩进
  const lines = [
    `/**`,
    ` * ${header}`,
    ` * 自动生成于 migrate-interview-bank.cjs`,
    ` */`,
    ``,
    `export interface ${typeName} {`,
  ]
  if (typeName === 'AlgoQuestion') {
    lines.push(
      `  id: string`,
      `  title: string`,
      `  problemId: number`,
      `  difficulty: 'easy' | 'medium' | 'hard'`,
      `  tags: string[]`,
      `  url: string`,
      `  platform: string`,
      `  category: string`,
      `  categoryTitle: string`,
      `}`
    )
  } else {
    lines.push(
      `  id: string`,
      `  title: string`,
      `  answer: string`,
      `  source: string`,
      `  difficulty: 'easy' | 'medium' | 'hard'`,
      `}`
    )
  }
  lines.push(``, `export const ${varName}: ${typeName}[] = [`)
  for (const item of data) {
    const json = JSON.stringify(item, null, 2)
    // 缩进每一行
    const indented = json.split('\n').map((l, i) => i === 0 ? l : '  ' + l).join('\n')
    lines.push(`  ${indented},`)
  }
  lines.push(`]`)
  return lines.join('\n')
}

function main() {
  console.log('=== 面试题库数据迁移 ===\n')

  const { algoBank, algoMeta } = migrateAlgoBank()
  const { interviewBank, interviewMeta } = migrateInterviewBank()

  // 去重
  const algoDedup = []
  const seenAlgo = new Set()
  for (const q of algoBank) {
    if (seenAlgo.has(q.id)) continue
    seenAlgo.add(q.id)
    algoDedup.push(q)
  }
  const interviewDedup = []
  const seenIv = new Set()
  for (const q of interviewBank) {
    if (seenIv.has(q.id)) continue
    seenIv.add(q.id)
    interviewDedup.push(q)
  }

  console.log(`\n=== 总计 ===`)
  console.log(`算法题: ${algoDedup.length} 题 / ${algoMeta.length} 分类`)
  console.log(`面试题: ${interviewDedup.length} 题 / ${interviewMeta.length} 分类`)
  console.log(`总分类: ${algoMeta.length + interviewMeta.length}`)

  // 写文件
  console.log(`\n=== 写入文件 ===`)
  writeTSFile('algo-bank.ts', generateTSContent('algoBank', 'AlgoQuestion', algoDedup, '算法题库（LeetCode 题目列表）'))

  // 拆分面试题到不同文件
  const grouped = {}
  for (const q of interviewDedup) {
    if (!grouped[q.source]) grouped[q.source] = []
    grouped[q.source].push(q)
  }
  // 写 ccpp-bank
  const ccppQs = [...(grouped['ccpp'] || []), ...(grouped['ccpp-extra'] || [])]
  if (ccppQs.length) {
    writeTSFile('ccpp-bank.ts', generateTSContent('ccppBank', 'InterviewQuestion', ccppQs, 'C/C++ 面试题'))
  }
  // 写 database-bank
  const dbQs = [...(grouped['database'] || []), ...(grouped['database-interview'] || []), ...(grouped['database-project'] || [])]
  if (dbQs.length) {
    writeTSFile('database-bank.ts', generateTSContent('databaseBank', 'InterviewQuestion', dbQs, '数据库面试题'))
  }
  // 写 vdb-bank
  const vdbQs = [...(grouped['vdb'] || []), ...(grouped['vdb-extra'] || [])]
  if (vdbQs.length) {
    writeTSFile('vdb-bank.ts', generateTSContent('vdbBank', 'InterviewQuestion', vdbQs, '向量数据库面试题'))
  }

  // 写 index.ts
  const indexContent = `/**
 * 面试题库聚合入口
 * 自动生成于 migrate-interview-bank.cjs
 */
import { algoBank, AlgoQuestion } from './algo-bank'
import { ccppBank, InterviewQuestion as CcppQ } from './ccpp-bank'
import { databaseBank, InterviewQuestion as DbQ } from './database-bank'
import { vdbBank, InterviewQuestion as VdbQ } from './vdb-bank'

export type { AlgoQuestion, CcppQ, DbQ, VdbQ }
export type InterviewQuestion = CcppQ

// 面试题库聚合
export const interviewBank: InterviewQuestion[] = [
  ...ccppBank,
  ...databaseBank,
  ...vdbBank
]

// 分类元数据
export interface BankCategory {
  id: string
  title: string
  icon: string
  desc?: string
  group: 'algorithm' | 'interview'
  count: number
  phase?: number
  order?: number
}

// 25 个算法分类
export const algoCategories: BankCategory[] = ${JSON.stringify(algoMeta.map(m => ({
  id: m.id, title: m.title, icon: m.icon, desc: m.desc, group: 'algorithm', count: m.count, phase: m.phase, order: m.order
})), null, 2)}

// 面试分类
export const interviewCategories: BankCategory[] = ${JSON.stringify(interviewMeta.map(m => ({
  id: m.id, title: m.title, icon: m.icon, group: 'interview', count: m.count
})), null, 2)}

// 辅助函数
export function getAlgoByCategory(catId: string): AlgoQuestion[] {
  return algoBank.filter(q => q.category === catId)
}

export function getInterviewBySource(source: string): InterviewQuestion[] {
  return interviewBank.filter(q => q.source === source)
}

export function searchAlgo(query: string): AlgoQuestion[] {
  const q = query.toLowerCase()
  return algoBank.filter(item =>
    item.title.toLowerCase().includes(q) ||
    item.tags.some(t => t.toLowerCase().includes(q)) ||
    item.categoryTitle.toLowerCase().includes(q)
  )
}

export function searchInterview(query: string): InterviewQuestion[] {
  const q = query.toLowerCase()
  return interviewBank.filter(item =>
    item.title.toLowerCase().includes(q) ||
    item.answer.toLowerCase().includes(q) ||
    item.source.toLowerCase().includes(q)
  )
}
`
  writeTSFile('index.ts', indexContent)

  console.log(`\n✅ 迁移完成！输出目录: ${OUTPUT_DIR}`)
}

main()
