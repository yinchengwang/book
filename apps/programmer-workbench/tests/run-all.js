/**
 * run-all.js —— 一键跑全部测试与校验
 *
 * 用法：node tests/run-all.js
 */

const { execFileSync } = require('child_process')
const path = require('path')

const ROOT = path.resolve(__dirname, '..')

const SUITES = [
  { name: '静态校验', file: 'tests/validate.js' },
  { name: 'core 层', file: 'tests/core.test.js' },
  { name: '工具箱', file: 'tests/tools.test.js' },
  { name: '今日/我的', file: 'tests/pages.test.js' },
  { name: '技术栈', file: 'tests/tech.test.js' },
  { name: '生活', file: 'tests/life.test.js' },
  { name: '项目/闪念/捕获', file: 'tests/p5.test.js' }
]

let failed = 0
const summary = []

SUITES.forEach(function (suite) {
  process.stdout.write('\n\x1b[1m▶ ' + suite.name + '\x1b[0m\n')
  try {
    const out = execFileSync(process.execPath, [path.join(ROOT, suite.file)], {
      cwd: ROOT,
      encoding: 'utf8'
    })
    // 只回显最后几行，保持输出紧凑
    const lines = out.trim().split('\n')
    console.log(lines.slice(-3).join('\n'))
    summary.push({ name: suite.name, ok: true })
  } catch (e) {
    const out = ((e.stdout || '') + (e.stderr || '')).trim()
    const lines = out.split('\n')
    console.log(lines.slice(-16).join('\n'))
    summary.push({ name: suite.name, ok: false })
    failed += 1
  }
})

console.log('\n' + '='.repeat(52))
summary.forEach(function (s) {
  console.log((s.ok ? '\x1b[32m✓\x1b[0m ' : '\x1b[31m✗\x1b[0m ') + s.name)
})

if (failed > 0) {
  console.log('\n\x1b[31m' + failed + ' 组失败\x1b[0m')
  process.exit(1)
}
console.log('\n\x1b[32m全部 ' + SUITES.length + ' 组通过\x1b[0m')
