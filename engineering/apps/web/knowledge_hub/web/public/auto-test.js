/**
 * 自动测试 v2 - 修复版
 * - 给每个页面更多时间
 * - 检查主内容区是否有真实内容（不只 Sidebar）
 */

;(async function autoTest() {
  const PAGES = [
    { name: 'Dashboard', path: '/' },
    { name: '题库练习', path: '/quiz' },
    { name: '复习计划', path: '/review' },
    { name: '面试追踪', path: '/interview-tracker' },
    { name: '模拟面试', path: '/mock-interview' },
    { name: '知识图谱', path: '/knowledge-graph' },
    { name: '学习路径', path: '/learning-path' },
    { name: '摘抄管理', path: '/excerpt' },
    { name: '差距分析', path: '/gap-analysis' },
    { name: '项目路线', path: '/project-roadmap' },
    { name: '设置', path: '/settings' }
  ]

  const results = []
  const errors = []

  const origError = console.error
  let currentErrors = []
  console.error = (...args) => {
    currentErrors.push(args.map(String).join(' '))
    origError.apply(console, args)
  }

  const handler = (e) => {
    currentErrors.push('UNHANDLED: ' + (e.reason?.message || e.reason))
  }
  window.addEventListener('unhandledrejection', handler)
  window.addEventListener('error', handler)

  for (const page of PAGES) {
    currentErrors = []
    const startTime = Date.now()

    history.pushState(null, '', page.path)
    window.dispatchEvent(new PopStateEvent('popstate'))

    // 等 5 秒让 lazy chunk 完全加载
    await new Promise(r => setTimeout(r, 5000))

    const elapsed = Date.now() - startTime

    // 获取主内容区（不是 sidebar）
    const mainContent = document.querySelector('.page-container, main')
    const sidebar = document.querySelector('.sidebar')

    const mainText = mainContent?.innerText?.trim() || ''
    const sidebarText = sidebar?.innerText?.trim() || ''

    // 判断主内容区是否有真实内容（> 50 字符且不只是 loading）
    const hasRealContent = mainText.length > 50 && !mainText.includes('加载中')
    const onlyLoading = mainText === '加载中...' || mainText === ''

    const errorDiv = document.querySelector('pre')
    const hasErrorUI = errorDiv && errorDiv.textContent?.includes('TypeError') ||
                        errorDiv && errorDiv.textContent?.includes('Error')

    let status, content
    if (hasErrorUI) {
      status = '❌'
      content = errorDiv?.textContent?.slice(0, 80) || 'Error UI'
    } else if (onlyLoading) {
      status = '⏳'
      content = '(还在加载)'
    } else if (hasRealContent) {
      status = '✅'
      content = mainText.replace(/\n/g, ' ').slice(0, 60)
    } else {
      status = '⚠️'
      content = mainText.replace(/\n/g, ' ').slice(0, 60) || '(空)'
    }

    results.push({
      page: page.name,
      path: page.path,
      status,
      elapsed: elapsed + 'ms',
      mainLen: mainText.length,
      errors: currentErrors.length,
      content,
      details: currentErrors.slice(0, 3)
    })

    if (currentErrors.length > 0 || hasErrorUI) {
      errors.push({ page: page.name, errors: currentErrors.slice(0, 3) })
    }
  }

  console.error = origError
  window.removeEventListener('unhandledrejection', handler)
  window.removeEventListener('error', handler)

  history.pushState(null, '', '/')
  window.dispatchEvent(new PopStateEvent('popstate'))

  console.clear()
  console.log('%c📋 Reading Radar H5 端自动化测试报告 v2', 'font-size:18px; font-weight:bold; color:#818cf8')
  console.log('='.repeat(80))

  const pass = results.filter(r => r.status === '✅').length
  const loading = results.filter(r => r.status === '⏳').length
  const warn = results.filter(r => r.status === '⚠️').length
  const fail = results.filter(r => r.status === '❌').length

  console.log(`\n总览: ${pass} ✅  ${loading} ⏳  ${warn} ⚠️  ${fail} ❌  (共 ${results.length} 页)\n`)

  results.forEach(r => {
    const color = r.status === '✅' ? '#10b981' :
                  r.status === '⏳' ? '#3b82f6' :
                  r.status === '⚠️' ? '#f59e0b' : '#ef4444'
    console.log(`%c${r.status} ${r.page.padEnd(8)} %c${r.path.padEnd(24)} %c${r.elapsed.padEnd(8)} 主区:${String(r.mainLen).padStart(4)}字符  ${r.content}`,
      `color:${color}; font-weight:bold`,
      'color:#94a3b8',
      'color:#64748b'
    )
    if (r.details && r.details.length > 0) {
      r.details.forEach(d => console.log(`   └─ ${d.slice(0, 120)}`))
    }
  })

  console.log('\n' + '='.repeat(80))
  if (errors.length > 0) {
    console.log(`\n%c❌ ${errors.length} 个页面有错误：`, 'color:#ef4444; font-weight:bold')
    errors.forEach(e => {
      console.log(`\n  ${e.page}:`)
      e.errors.forEach(err => console.log(`    - ${err.slice(0, 200)}`))
    })
  }

  if (loading > 0) {
    console.log(`\n%c⏳ ${loading} 个页面还在加载（可能 chunk 比较大，需要更多时间）`, 'color:#3b82f6')
  }

  console.log('\n%c复制以上结果发给 Claude', 'color:#818cf8; font-style:italic')

  return { pass, loading, warn, fail, results, errors }
})()
