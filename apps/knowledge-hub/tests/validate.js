#!/usr/bin/env node
/**
 * validate.js —— 原生小程序静态校验
 *
 * 五项检查：
 *   1. JS 语法（vm 编译）
 *   2. JSON 合法性
 *   3. 页面注册一致（app.json ↔ 磁盘，双向）
 *   4. 组件引用存在（usingComponents）
 *   5. WXML handler 交叉校验（bind / catch 绑定的方法必须在页面 JS 中存在）
 *
 * 用法：node tests/validate.js
 */

const fs = require('fs')
const path = require('path')
const vm = require('vm')
const stub = require('./wx-stub.js')

const ROOT = path.resolve(__dirname, '..')

// 忽略依赖与测试自身
const IGNORE_DIRS = ['node_modules', '.git', 'miniprogram_npm', 'tests', '.archive']

const errors = []
const warnings = []

function fail (msg) { errors.push(msg) }
function warn (msg) { warnings.push(msg) }

/** 递归收集文件，返回相对 ROOT 的 posix 路径 */
function walk (dir, out) {
  const abs = path.join(ROOT, dir)
  if (!fs.existsSync(abs)) return out
  fs.readdirSync(abs, { withFileTypes: true }).forEach(entry => {
    const rel = dir ? dir + '/' + entry.name : entry.name
    if (entry.isDirectory()) {
      if (IGNORE_DIRS.indexOf(entry.name) >= 0) return
      walk(rel, out)
    } else {
      out.push(rel)
    }
  })
  return out
}

const files = walk('', [])
const jsFiles = files.filter(f => f.endsWith('.js'))
const jsonFiles = files.filter(f => f.endsWith('.json'))
const wxmlFiles = files.filter(f => f.endsWith('.wxml'))

// ---------- 1. JS 语法 ----------
function checkJsSyntax () {
  jsFiles.forEach(f => {
    const src = fs.readFileSync(path.join(ROOT, f), 'utf8')
    try {
      new vm.Script(src, { filename: f })
    } catch (e) {
      fail('[JS 语法] ' + f + ' → ' + e.message)
    }
  })
}

// ---------- 2. JSON 合法性 ----------
function checkJson () {
  jsonFiles.forEach(f => {
    const raw = fs.readFileSync(path.join(ROOT, f), 'utf8')
    try {
      JSON.parse(raw)
    } catch (e) {
      fail('[JSON] ' + f + ' → ' + e.message)
    }
  })
}

// ---------- 3. 页面注册一致 ----------
function checkPages () {
  const appJsonPath = path.join(ROOT, 'app.json')
  if (!fs.existsSync(appJsonPath)) {
    fail('[页面] 缺少 app.json')
    return
  }
  const appJson = JSON.parse(fs.readFileSync(appJsonPath, 'utf8'))
  const pages = appJson.pages || []

  if (pages.length === 0) fail('[页面] app.json 未注册任何页面')

  // 正向：每个注册页必须有 js / wxml / json
  pages.forEach(p => {
    ;['js', 'wxml', 'json'].forEach(ext => {
      const f = p + '.' + ext
      if (!fs.existsSync(path.join(ROOT, f))) fail('[页面] 注册页缺少文件: ' + f)
    })
    if (!fs.existsSync(path.join(ROOT, p + '.wxss'))) {
      warn('[页面] 缺少样式文件（可选）: ' + p + '.wxss')
    }
  })

  // tabBar 引用必须在 pages 内
  const tabBar = appJson.tabBar
  if (tabBar && tabBar.list) {
    tabBar.list.forEach(item => {
      if (pages.indexOf(item.pagePath) < 0) {
        fail('[tabBar] pagePath 未注册: ' + item.pagePath)
      }
    })
  }

  // 反向：pages/ 下的页面 js 都应在 app.json 注册
  const pageJs = jsFiles.filter(f =>
    f.startsWith('pages/') && f.endsWith('.js') && !f.endsWith('.test.js')
  )
  pageJs.forEach(f => {
    const route = f.replace(/\.js$/, '')
    if (pages.indexOf(route) < 0) {
      fail('[页面] 磁盘存在但未注册: ' + route)
    }
  })
}

// ---------- 4. 组件引用存在 ----------
function checkComponents () {
  jsonFiles.forEach(f => {
    let cfg
    try {
      cfg = JSON.parse(fs.readFileSync(path.join(ROOT, f), 'utf8'))
    } catch (e) {
      return
    }
    const using = cfg.usingComponents
    if (!using || typeof using !== 'object') return

    const dir = path.posix.dirname(f)
    Object.keys(using).forEach(name => {
      const ref = using[name]
      if (!ref) return
      if (ref.startsWith('plugin://')) return

      let target
      if (ref.startsWith('/')) {
        target = ref.slice(1)
      } else {
        target = path.posix.normalize(path.posix.join(dir, ref))
      }
      if (!fs.existsSync(path.join(ROOT, target + '.js'))) {
        fail('[组件] ' + f + ' 引用 ' + name + ' → 找不到 ' + target + '.js')
      }
    })
  })
}

// ---------- 5. WXML handler 交叉校验 ----------
const HANDLER_RE = /\b(?:capture-bind|capture-catch|bind|catch)[:-]?([a-zA-Z]+)\s*=\s*"([^"{}]+)"/g

/** 加载页面/组件模块，取出方法名集合 */
function loadMethods (jsFile) {
  const abs = path.join(ROOT, jsFile)
  if (!fs.existsSync(abs)) return null

  let captured = null
  const prev = {
    Page: global.Page,
    Component: global.Component,
    Behavior: global.Behavior
  }
  global.Page = function (o) { captured = o }
  global.Component = function (o) { captured = o }
  global.Behavior = function (o) { captured = o }

  try {
    delete require.cache[require.resolve(abs)]
    require(abs)
  } catch (e) {
    captured = null
  } finally {
    global.Page = prev.Page
    global.Component = prev.Component
    global.Behavior = prev.Behavior
  }

  if (!captured) return null

  const names = new Set()
  Object.keys(captured).forEach(k => {
    if (typeof captured[k] === 'function') names.add(k)
  })
  // 组件方法在 methods 里
  if (captured.methods && typeof captured.methods === 'object') {
    Object.keys(captured.methods).forEach(k => {
      if (typeof captured.methods[k] === 'function') names.add(k)
    })
  }
  return names
}

function checkHandlers () {
  wxmlFiles.forEach(f => {
    const src = fs.readFileSync(path.join(ROOT, f), 'utf8')
    const jsFile = f.replace(/\.wxml$/, '.js')

    const handlers = new Set()
    let m
    HANDLER_RE.lastIndex = 0
    while ((m = HANDLER_RE.exec(src)) !== null) {
      handlers.add(m[2].trim())
    }
    if (handlers.size === 0) return

    const methods = loadMethods(jsFile)
    if (!methods) {
      fail('[handler] ' + f + ' 无法加载对应 JS: ' + jsFile)
      return
    }
    handlers.forEach(h => {
      if (!methods.has(h)) {
        fail('[handler] ' + f + ' 绑定 ' + h + '() 但 ' + jsFile + ' 中不存在')
      }
    })
  })
}

// ---------- 执行 ----------
stub.install()
checkJsSyntax()
checkJson()
checkPages()
checkComponents()
checkHandlers()

const rel = files.length
console.log('')
console.log('校验范围: ' + rel + ' 个文件（js ' + jsFiles.length + ' / json ' + jsonFiles.length + ' / wxml ' + wxmlFiles.length + '）')

if (warnings.length) {
  console.log('')
  warnings.forEach(w => console.log('  \x1b[33mWARN\x1b[0m ' + w))
}

if (errors.length) {
  console.log('')
  errors.forEach(e => console.log('  \x1b[31mFAIL\x1b[0m ' + e))
  console.log('')
  console.log('\x1b[31m校验失败\x1b[0m：' + errors.length + ' 个错误，' + warnings.length + ' 个警告')
  process.exit(1)
}

console.log('')
console.log('\x1b[32m校验通过\x1b[0m：0 个错误，' + warnings.length + ' 个警告')
