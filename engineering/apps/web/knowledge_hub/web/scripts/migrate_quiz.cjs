/**
 * 数据迁移脚本 - 把旧版 quiz 题库转为新版格式
 *
 * 用法：node migrate-quiz.js
 * 输出：src/data/quiz-*.ts
 */

const fs = require('fs')
const path = require('path')
const vm = require('vm')

const SOURCE_DIR = path.join(__dirname, '..', '..', '..', 'project', 'reading-radar', 'data', 'quiz', 'questions')
const OUTPUT_DIR = path.join(__dirname, '..', '..', 'src', 'data')

// 字段映射
const TYPE_MAP = {
  'choice': '选择题',
  'true_false': '判断题',
  'fill_blank': '填空题',
  'predict_output': '编程题',
  'short_answer': '简答题',
  'code': '编程题'
}

const DIFFICULTY_MAP = {
  'basic': '入门',
  'intermediate': '中级',
  'advanced': '高级',
  'expert': '专家'
}

// 转换单个题目
function convertQuestion(q, stack) {
  let description = q.scenario || ''
  if (q.code) {
    description += (description ? '\n\n' : '') + '```\n' + q.code + '\n```'
  }

  const title = q.stem || q.title || '题目'

  let options = q.options
  if (q.type === 'true_false') {
    options = ['正确', '错误']
  } else if (!options || options.length === 0) {
    options = undefined
  }

  return {
    id: q.id,
    title: title.slice(0, 200),
    description: description.slice(0, 2000),
    category: TYPE_MAP[q.type] || '选择题',
    difficulty: DIFFICULTY_MAP[q.difficulty] || '中级',
    domain: stack + (q.quadrant ? ' · ' + q.quadrant : ''),
    options,
    answer: String(q.answer),
    explanation: q.explanation || '',
    timeEstimate: q.difficulty === 'basic' ? 3 : q.difficulty === 'intermediate' ? 8 : 15,
    tags: [stack, q.quadrant, q.ring, q.difficulty].filter(Boolean)
  }
}

// 提取一个旧文件的 QUESTION_BANK.x
function extractQuestions(filePath, stack) {
  const content = fs.readFileSync(filePath, 'utf-8')

  // 准备沙箱
  const sandbox = {
    console
  }
  vm.createContext(sandbox)

  // 注入 QUESTION_BANK 声明 + 旧文件
  const script = `
    var QUESTION_BANK = {};
    ${content}
    QUESTION_BANK;
  `

  try {
    const result = vm.runInContext(script, sandbox, { filename: filePath })

    if (!result || !result[stack]) {
      console.log(`  ${path.basename(filePath)}: no data for stack=${stack}, all keys: ${Object.keys(result || {}).join(',')}`)
      return []
    }

    const all = []
    for (const ring in result[stack]) {
      const qs = result[stack][ring]
      if (Array.isArray(qs)) {
        for (const q of qs) {
          all.push(convertQuestion(q, stack))
        }
      }
    }
    console.log(`  ${path.basename(filePath)}: ${all.length} 题`)
    return all
  } catch (e) {
    console.warn(`  ${filePath} 解析失败: ${e.message}`)
    return []
  }
}

function main() {
  console.log('开始迁移 quiz 题库...')
  console.log('源目录:', SOURCE_DIR)
  console.log('是否存在:', fs.existsSync(SOURCE_DIR))

  if (!fs.existsSync(OUTPUT_DIR)) {
    fs.mkdirSync(OUTPUT_DIR, { recursive: true })
  }

  // 各 stack
  const stacks = ['c', 'cpp', 'db', 'ds', 'linux', 'py', 'vdb']
  const byStack = {}
  const seenIds = new Set()  // 全局去重

  for (const stack of stacks) {
    const stackDir = path.join(SOURCE_DIR, stack)
    console.log(`\n[${stack}] 目录:`, stackDir, '存在:', fs.existsSync(stackDir))
    if (!fs.existsSync(stackDir)) continue

    const files = fs.readdirSync(stackDir).filter(f => f.startsWith('quiz-questions-'))
    console.log(`  文件数:`, files.length, files)
    const stackQuestions = []

    for (const file of files) {
      const filePath = path.join(stackDir, file)
      const qs = extractQuestions(filePath, stack)
      // 全局去重 - 重复 id 加后缀
      const deduped = qs.map((q) => {
        let id = q.id
        let suffix = 0
        while (seenIds.has(id)) {
          suffix++
          id = `${q.id}-d${suffix}`
        }
        seenIds.add(id)
        return { ...q, id }
      })
      stackQuestions.push(...deduped)
    }

    if (stackQuestions.length > 0) {
      byStack[stack] = stackQuestions
      console.log(`  ${stack}: ${stackQuestions.length} 题`)
    } else {
      console.log(`  ${stack}: 0 题 (跳过)`)
    }
  }

  // 写入文件
  for (const stack in byStack) {
    const filePath = path.join(OUTPUT_DIR, `quiz-${stack}.ts`)
    const content = `// 自动生成 - 题库数据
// 来源：旧版 reading-radar (${stack} 分类)
// 共 ${byStack[stack].length} 道题
// 生成时间：${new Date().toISOString()}

import type { QuizQuestion } from '@/stores/quizStore'

export const ${stack}Questions: QuizQuestion[] = ${JSON.stringify(byStack[stack], null, 2)}
`
    fs.writeFileSync(filePath, content, 'utf-8')
    console.log(`生成: src/data/quiz-${stack}.ts`)
  }

  // 生成索引
  const stacks2 = Object.keys(byStack)
  const indexContent = `// 自动生成 - 题库索引
import type { QuizQuestion } from '@/stores/quizStore'

${stacks2.map(s => `import { ${s}Questions } from './quiz-${s}'`).join('\n')}

export const allQuizQuestions: QuizQuestion[] = [
${stacks2.map(s => `  ...${s}Questions,`).join('\n')}
]

export const quizByStack: Record<string, QuizQuestion[]> = {
${stacks2.map(s => `  ${s}: ${s}Questions,`).join('\n')}
}

export const quizStats = {
  total: ${stacks2.reduce((sum, s) => sum + byStack[s].length, 0)},
${stacks2.map(s => `  ${s}: ${byStack[s].length},`).join('\n')}
}
`
  fs.writeFileSync(path.join(OUTPUT_DIR, 'quiz-index.ts'), indexContent, 'utf-8')
  console.log('生成: src/data/quiz-index.ts')

  // 统计
  const total = stacks2.reduce((sum, s) => sum + byStack[s].length, 0)
  console.log(`\n总计: ${total} 道题`)

  // 体积
  const totalSize = Object.values(byStack).reduce((sum, qs) => {
    return sum + JSON.stringify(qs).length
  }, 0)
  console.log(`原始数据体积: ${(totalSize / 1024 / 1024).toFixed(2)} MB`)
}

main()
