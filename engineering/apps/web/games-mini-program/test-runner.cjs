// Simple test runner to validate storage logic without vitest
const path = require('path')
const fs = require('fs')

// Set up in-memory mock for Taro
const taroStore = {}

const taroMock = {
  getStorageSync: (key) => taroStore[key],
  setStorageSync: (key, value) => { taroStore[key] = value },
  removeStorageSync: (key) => { delete taroStore[key] }
}

// Read storage.ts source and eval it with mocked Taro
const storageSrc = fs.readFileSync(
  path.join(__dirname, 'src/utils/storage.ts'),
  'utf8'
)

const tsCode = storageSrc
  // Strip imports we don't need
  .replace(/^import .*$/gm, '')
  .replace(/^export interface[\s\S]*?\n\}/gm, '')
  .replace(/^export type[\s\S]*?\n/gm, '')

// Compile TypeScript-to-JS via simple regex substitutions
let jsCode = tsCode
  .replace(/export function/g, 'function')
  .replace(/export interface[\s\S]*?\n\}/g, '')
  .replace(/export type[\s\S]*?\n/g, '')
  .replace(/: number/g, '')
  .replace(/: boolean/g, '')
  .replace(/: string/g, '')
  .replace(/: AchievementId/g, '')
  .replace(/: Achievement \| null/g, '')
  .replace(/: Achievement\[\]/g, '')
  .replace(/: AchievementData/g, '')
  .replace(/: AchievementProgress/g, '')
  .replace(/: T\b/g, '')
  .replace(/<T>/g, '')
  .replace(/<Game2048Data>/g, '')
  .replace(/<SnakeData>/g, '')
  .replace(/<SudokuData>/g, '')
  .replace(/<Match3Data>/g, '')
  .replace(/<AchievementData>/g, '')
  .replace(/<AchievementProgress>/g, '')
  .replace(/as T/g, '')
  .replace(/as AchievementProgress/g, '')
  .replace(/as Achievement\[\]/g, '')

// Wrap in a module-like scope with our mocked Taro
const wrapped = `
const Taro = ${JSON.stringify(taroMock)};
const console = { warn: () => {}, log: () => {}, error: () => {} };
${jsCode}

module.exports = {
  get2048BestScore, update2048BestScore,
  getSnakeBestScore, updateSnakeBestScore,
  getAchievements, recordAchievementProgress, unlockAchievement
};
`

const Module = require('module')
const m = new Module('storage-test')
m.filename = path.join(__dirname, 'storage-test.js')
m.paths = Module._nodeModulePaths(m.filename)
m._compile(wrapped, m.filename)
const storage = m.exports

// Run tests
let pass = 0, fail = 0
const log = (ok, name) => {
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}`)
  if (ok) pass++; else fail++
}

// Wipe store
Object.keys(taroStore).forEach(k => delete taroStore[k])

// Test 1: new best score persisted
storage.update2048BestScore(0)
storage.update2048BestScore(2048)
log(storage.get2048BestScore() === 2048, '新最高分被持久化')

// Test 2: lower score does not overwrite
storage.update2048BestScore(4096)
storage.update2048BestScore(2048)
log(storage.get2048BestScore() === 4096, '低于最高分的提交不会覆盖')

console.log(`\nResults: ${pass} passed, ${fail} failed`)
process.exit(fail > 0 ? 1 : 0)
