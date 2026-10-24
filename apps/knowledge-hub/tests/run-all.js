/**
 * run-all.js —— 一键跑全部测试与校验
 *
 * 用法：node tests/run-all.js
 *
 * 自动发现 tests/*.test.js，并始终先跑静态校验（validate.js）。
 * 新增测试套件只需在 tests/ 下放置 *.test.js，无需手动登记。
 */

const fs = require('fs')
const path = require('path')
const { execFileSync } = require('child_process')

const ROOT = path.resolve(__dirname, '..')

const SUITES = [{ name: '静态校验', file: 'tests/validate.js' }]

fs.readdirSync(path.join(ROOT, 'tests')).forEach(function (f) {
  if (/^.+\.test\.js$/.test(f)) {
    SUITES.push({ name: f.replace(/\.js$/, ''), file: 'tests/' + f })
  }
})

let failed = 0
const summary = []

SUITES.forEach(function (suite) {
  process.stdout.write('\n\x1b[1m▶ ' + suite.name + '\x1b[0m\n')
  try {
    const out = execFileSync(process.execPath, [path.join(ROOT, suite.file)], {
      cwd: ROOT,
      encoding: 'utf8'
    })
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
