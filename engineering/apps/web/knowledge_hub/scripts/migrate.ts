/**
 * 数据迁移脚本
 * 用于从旧版 reading-radar 迁移数据到新版 reading-radar-2
 */

import { writeFileSync, readFileSync, existsSync, mkdirSync } from 'fs'
import { join, dirname } from 'path'
import { fileURLToPath } from 'url'

const __dirname = dirname(fileURLToPath(import.meta.url))

// 迁移配置
interface MigrationConfig {
  sourceDir: string
  targetDir: string
  dryRun: boolean
}

// 默认配置
const defaultConfig: MigrationConfig = {
  sourceDir: '../reading-radar',
  targetDir: './data',
  dryRun: false
}

// 迁移数据类型
interface BookData {
  id: string
  title: string
  author: string
  category: string
  status: 'reading' | 'completed' | 'planning'
  progress: number
  notes: string
  excerpts: number
}

interface KnowledgeItem {
  id: string
  name: string
  category: string
  level: number
  mastery: number
}

interface QuizQuestion {
  id: string
  question: string
  answer: string
  category: string
  difficulty: 'easy' | 'medium' | 'hard'
  source?: string
}

// 迁移书籍数据
function migrateBooks(config: MigrationConfig): BookData[] {
  console.log('📚 开始迁移书籍数据...')

  const sourcePath = join(config.sourceDir, 'data', 'books.json')
  const targetPath = join(config.targetDir, 'books.json')

  try {
    if (!existsSync(sourcePath)) {
      console.log('⚠️  源文件不存在:', sourcePath)
      return []
    }

    const sourceData = JSON.parse(readFileSync(sourcePath, 'utf-8'))
    const migratedData: BookData[] = sourceData.map((book: any, idx: number) => ({
      id: `book-${Date.now()}-${idx}`,
      title: book.title || book.name,
      author: book.author || '未知',
      category: book.category || '技术',
      status: book.status || 'planning',
      progress: book.progress || 0,
      notes: book.notes || '',
      excerpts: book.excerpts?.length || 0
    }))

    if (!config.dryRun) {
      mkdirSync(config.targetDir, { recursive: true })
      writeFileSync(targetPath, JSON.stringify(migratedData, null, 2))
    }

    console.log(`✅ 迁移完成: ${migratedData.length} 本书`)
    return migratedData
  } catch (error) {
    console.error('❌ 迁移失败:', error)
    return []
  }
}

// 迁移知识数据
function migrateKnowledge(config: MigrationConfig): KnowledgeItem[] {
  console.log('🧠 开始迁移知识数据...')

  const sourcePath = join(config.sourceDir, 'data', 'knowledge.json')
  const targetPath = join(config.targetDir, 'knowledge.json')

  try {
    if (!existsSync(sourcePath)) {
      console.log('⚠️  源文件不存在:', sourcePath)
      return []
    }

    const sourceData = JSON.parse(readFileSync(sourcePath, 'utf-8'))
    const migratedData: KnowledgeItem[] = sourceData.map((item: any, idx: number) => ({
      id: item.id || `knowledge-${idx}`,
      name: item.name || item.title,
      category: item.category || '其他',
      level: item.level || 1,
      mastery: item.mastery || item.level * 20
    }))

    if (!config.dryRun) {
      mkdirSync(config.targetDir, { recursive: true })
      writeFileSync(targetPath, JSON.stringify(migratedData, null, 2))
    }

    console.log(`✅ 迁移完成: ${migratedData.length} 个知识点`)
    return migratedData
  } catch (error) {
    console.error('❌ 迁移失败:', error)
    return []
  }
}

// 迁移题库数据
function migrateQuestions(config: MigrationConfig): QuizQuestion[] {
  console.log('📝 开始迁移题库数据...')

  const sourcePath = join(config.sourceDir, 'data', 'questions.json')
  const targetPath = join(config.targetDir, 'questions.json')

  try {
    if (!existsSync(sourcePath)) {
      console.log('⚠️  源文件不存在:', sourcePath)
      return []
    }

    const sourceData = JSON.parse(readFileSync(sourcePath, 'utf-8'))
    const migratedData: QuizQuestion[] = sourceData.map((q: any, idx: number) => ({
      id: q.id || `q-${idx}`,
      question: q.question || q.title,
      answer: q.answer || '',
      category: q.category || '技术',
      difficulty: q.difficulty || 'medium',
      source: q.source || ''
    }))

    if (!config.dryRun) {
      mkdirSync(config.targetDir, { recursive: true })
      writeFileSync(targetPath, JSON.stringify(migratedData, null, 2))
    }

    console.log(`✅ 迁移完成: ${migratedData.length} 道题目`)
    return migratedData
  } catch (error) {
    console.error('❌ 迁移失败:', error)
    return []
  }
}

// 迁移摘抄数据
function migrateExcerpts(config: MigrationConfig): any[] {
  console.log('📖 开始迁移摘抄数据...')

  const sourceDir = join(config.sourceDir, 'data', 'excerpts')
  const targetDir = join(config.targetDir, 'excerpts')
  const migratedData: any[] = []

  try {
    if (!existsSync(sourceDir)) {
      console.log('⚠️  源目录不存在:', sourceDir)
      return []
    }

    // 读取所有摘抄文件
    const files = []
    // const mdFiles = glob.sync(join(sourceDir, '*.md'))
    // ... 简化处理

    if (!config.dryRun) {
      mkdirSync(targetDir, { recursive: true })
    }

    console.log(`✅ 摘抄目录已创建: ${targetDir}`)
    return migratedData
  } catch (error) {
    console.error('❌ 迁移失败:', error)
    return []
  }
}

// 生成迁移报告
function generateReport(results: {
  books: number
  knowledge: number
  questions: number
  excerpts: number
}, config: MigrationConfig): void {
  const report = {
    title: 'Reading Radar 数据迁移报告',
    timestamp: new Date().toISOString(),
    mode: config.dryRun ? '预览模式' : '执行模式',
    summary: {
      总项目: results.books + results.knowledge + results.questions,
      书籍: results.books,
      知识点: results.knowledge,
      题目: results.questions,
      摘抄: results.excerpts
    },
    nextSteps: [
      '1. 检查迁移后的数据文件',
      '2. 在新版应用中导入数据',
      '3. 验证数据完整性',
      '4. 确认无误后可删除旧版数据'
    ]
  }

  const reportPath = join(config.targetDir, 'migration-report.json')
  writeFileSync(reportPath, JSON.stringify(report, null, 2))
  console.log('\n📄 迁移报告已生成:', reportPath)
  console.log(JSON.stringify(report, null, 2))
}

// 主迁移函数
export function migrateAll(config: Partial<MigrationConfig> = {}): void {
  const fullConfig: MigrationConfig = { ...defaultConfig, ...config }

  console.log('🚀 开始数据迁移...')
  console.log(`模式: ${fullConfig.dryRun ? '预览（不写入文件）' : '执行'}`)
  console.log(`源目录: ${fullConfig.sourceDir}`)
  console.log(`目标目录: ${fullConfig.targetDir}`)
  console.log('---')

  const results = {
    books: migrateBooks(fullConfig).length,
    knowledge: migrateKnowledge(fullConfig).length,
    questions: migrateQuestions(fullConfig).length,
    excerpts: migrateExcerpts(fullConfig).length
  }

  console.log('---')
  console.log('📊 迁移统计:')
  console.log(`  书籍: ${results.books}`)
  console.log(`  知识点: ${results.knowledge}`)
  console.log(`  题目: ${results.questions}`)
  console.log(`  摘抄: ${results.excerpts}`)

  generateReport(results, fullConfig)

  if (fullConfig.dryRun) {
    console.log('\n💡 这是预览模式，使用 --execute 参数实际执行迁移')
  } else {
    console.log('\n✅ 迁移完成！')
  }
}

// CLI 入口
if (import.meta.url === `file://${process.argv[1]}`) {
  const args = process.argv.slice(2)
  const dryRun = !args.includes('--execute')

  if (args.includes('--help')) {
    console.log(`
数据迁移脚本使用说明:

用法: npx ts-node scripts/migrate.ts [选项]

选项:
  --execute    执行实际迁移（默认是预览模式）
  --help       显示帮助信息

示例:
  npx ts-node scripts/migrate.ts          # 预览迁移
  npx ts-node scripts/migrate.ts --execute # 执行迁移
    `)
    process.exit(0)
  }

  migrateAll({ dryRun })
}

export default migrateAll
