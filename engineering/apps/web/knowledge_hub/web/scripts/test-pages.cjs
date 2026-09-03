// 直接用 playwright 跑测试
const { chromium } = require('playwright')

const PAGES = [
  { name: 'Dashboard', path: '/' },
  { name: '题库练习', path: '/quiz' },
  { name: '复习计划', path: '/review' },
  { name: '面试追踪', path: '/interview-tracker' },
  { name: '模拟面试', path: '/mock-interview' },
  { name: '知识图谱', path: '/knowledge-graph' },
  { name: '学习路径', path: '/learning-path' },
  { name: '图解系列', path: '/learn-deep' },
  { name: '摘抄管理', path: '/excerpt' },
  { name: '差距分析', path: '/gap-analysis' },
  { name: '项目路线', path: '/project-roadmap' },
  { name: '设置', path: '/settings' }
]

;(async () => {
  console.log('启动浏览器...')
  // 优先用默认 chromium（无需安装 chrome），失败时回退到 msedge
  const browser = await chromium.launch({
    headless: true,
    args: ['--no-sandbox', '--disable-dev-shm-usage', '--disable-gpu']
  }).catch(async () => {
    return chromium.launch({
      headless: true,
      executablePath: 'C:\\Users\\yinch\\AppData\\Local\\ms-playwright\\chromium_headless_shell-1223\\chrome-headless-shell-win64\\chrome-headless-shell.exe',
      args: ['--no-sandbox', '--disable-dev-shm-usage', '--disable-gpu']
    })
  })
  const ctx = await browser.newContext()
  const page = await ctx.newPage()
  const errors = []
  page.on('pageerror', e => errors.push(`PAGE: ${e.message}`))
  page.on('console', m => {
    if (m.type() === 'error' && !m.text().includes('legacy-js-api')) {
      errors.push(`CONSOLE: ${m.text().slice(0, 150)}`)
    }
  })

  let pass = 0, fail = 0
  const results = []

  for (const p of PAGES) {
    const beforeErrors = errors.length
    try {
      await page.goto(`http://localhost:5173${p.path}`, { waitUntil: 'networkidle', timeout: 30000 })
      await page.waitForTimeout(2000)
      const mainText = await page.evaluate(() => {
        const main = document.querySelector('.page-container')
        return main ? main.innerText.trim() : ''
      })
      const hasContent = mainText.length > 20
      const hasNewError = errors.length > beforeErrors
      const hasError = hasNewError
      const status = hasError ? '❌' : (hasContent ? '✅' : '⚠️')
      if (status === '✅') pass++
      else fail++
      results.push({ name: p.name, status, len: mainText.length, err: hasError ? errors[errors.length - 1] : null })
      console.log(`  ${status} ${p.name.padEnd(8)} (${mainText.length}字) ${hasError ? errors[errors.length - 1].slice(0, 60) : ''}`)
    } catch (e) {
      fail++
      results.push({ name: p.name, status: '❌', err: e.message.slice(0, 100) })
      console.log(`  ❌ ${p.name} - ${e.message.slice(0, 80)}`)
    }
  }

  // 测试 Quiz 各项功能
  console.log('\n--- Quiz 完整流程 ---')
  try {
    // 1. 默认进入计划页
    await page.goto('http://localhost:5173/quiz', { waitUntil: 'networkidle' })
    await page.waitForTimeout(2000)
    const startPlanBtn = await page.locator('button:has-text("开始今日计划")').count()
    console.log(`  ${startPlanBtn > 0 ? '✅' : '❌'} 计划页 - 开始今日计划按钮`)

    const statsCards = await page.locator('.stats-row .stat-item').count()
    console.log(`  ${statsCards >= 4 ? '✅' : '❌'} 计划页 - 统计卡片 (${statsCards}个)`)

    // 2. 题库浏览
    const browseBtn = page.locator('text=/浏览题库/').first()
    if (await browseBtn.count() > 0) {
      await browseBtn.click()
      await page.waitForTimeout(1500)
      const quizChips = await page.locator('.page-container .chip').count()
      const listItems = await page.locator('.page-container .question-list-item').count()
      console.log(`  ${quizChips >= 5 ? '✅' : '❌'} 题库 - 分类 chips (${quizChips}个)`)
      console.log(`  ${listItems > 0 ? '✅' : '❌'} 题库 - 题目列表项 (${listItems}个)`)
    }

    // 3. 开始答题
    const startBtn = page.locator('button:has-text("开始前 20 题")').first()
    if (await startBtn.count() > 0) {
      await startBtn.click()
      await page.waitForTimeout(1500)
      // 用精确的 .option（不是 .options-list）
      const opt1 = page.locator('.page-container .option').first()
      await opt1.click()
      await page.waitForTimeout(800)

      const isSelected = await opt1.evaluate(el => el.classList.contains('selected'))
      console.log(`  ${isSelected ? '✅' : '❌'} 答题 - 选项点击 (selected: ${isSelected})`)

      // 提交
      const submitBtn = page.locator('button:not([disabled])').filter({ hasText: /提交/ }).first()
      if (await submitBtn.count() > 0) {
        await submitBtn.click()
        await page.waitForTimeout(1000)
        const feedback = await page.locator('.result-feedback').count()
        const nextBtn = await page.locator('button').filter({ hasText: /下一题|完成/ }).count()
        console.log(`  ${feedback > 0 ? '✅' : '❌'} 答题 - 提交后反馈 (${feedback}个反馈, ${nextBtn}个下一题按钮)`)
      }
    }

    // 4. 错题本
    await page.goto('http://localhost:5173/quiz', { waitUntil: 'networkidle' })
    await page.waitForTimeout(1500)
    const wrongBtn = page.locator('text=/错题本/').first()
    if (await wrongBtn.count() > 0) {
      await wrongBtn.click()
      await page.waitForTimeout(1500)
      const wrongMode = await page.locator('text=/错题本/').count()
      console.log(`  ${wrongMode > 0 ? '✅' : '❌'} 错题本页面`)
    }

    // 5. 学习统计
    const statsBtn = page.locator('text=/学习统计/').first()
    if (await statsBtn.count() > 0) {
      await statsBtn.click()
      await page.waitForTimeout(1500)
      const statsCards = await page.locator('.stats-cards .stats-card').count()
      console.log(`  ${statsCards >= 4 ? '✅' : '❌'} 学习统计 (${statsCards}张卡)`)
    }
  } catch (e) {
    console.log(`  ❌ Quiz 流程失败: ${e.message.slice(0, 100)}`)
  }

  // 测试 Settings 交互
  console.log('\n--- Settings 交互 ---')
  try {
    await page.goto('http://localhost:5173/settings', { waitUntil: 'networkidle' })
    await page.waitForTimeout(1500)
    const switches = await page.locator('[role="switch"]').count()
    console.log(`  ${switches >= 1 ? '✅' : '❌'} Settings 开关 (${switches}个)`)

    if (switches >= 1) {
      const before = await page.locator('[role="switch"]').first().getAttribute('aria-checked')
      await page.locator('[role="switch"]').first().click()
      await page.waitForTimeout(300)
      const after = await page.locator('[role="switch"]').first().getAttribute('aria-checked')
      console.log(`  ${before !== after ? '✅' : '⚠️'} Settings 开关切换 (${before}→${after})`)
    }
  } catch (e) {
    console.log(`  ❌ Settings 失败: ${e.message}`)
  }

  // 测试 Interview Tracker 交互
  console.log('\n--- Interview Tracker 交互 ---')
  try {
    await page.goto('http://localhost:5173/interview-tracker', { waitUntil: 'networkidle' })
    await page.waitForTimeout(1500)
    const companyCards = await page.locator('.page-container [class*="company"]').count()
    console.log(`  ${companyCards > 0 ? '✅' : '❌'} Interview 公司卡片 (${companyCards}个)`)

    const addBtn = page.locator('button').filter({ hasText: /新|添加|\+/ }).first()
    if (await addBtn.count() > 0) {
      await addBtn.click()
      await page.waitForTimeout(1000)
      const form = await page.locator('[class*="form"], [class*="modal"], [class*="drawer"]').count()
      console.log(`  ${form > 0 ? '✅' : '⚠️'} Interview 新增表单`)
    }
  } catch (e) {
    console.log(`  ❌ Interview 失败: ${e.message}`)
  }

  // 测试图解系列
  console.log('\n--- 图解系列 ---')
  try {
    await page.goto('http://localhost:5173/learn-deep', { waitUntil: 'networkidle' })
    await page.waitForTimeout(3000)

    // 抓详细错误
    const errorDetails = await page.evaluate(() => {
      const root = document.getElementById('root')
      const text = root?.innerText?.slice(0, 500) || ''
      return text
    })
    console.log(`  Page text: ${errorDetails.slice(0, 200)}`)

    const articleCards = await page.locator('.article-card').count()
    console.log(`  ${articleCards > 0 ? '✅' : '❌'} LearnDeep 文章卡片 (${articleCards}个)`)

    const categories = await page.locator('.chip').count()
    console.log(`  ${categories >= 5 ? '✅' : '❌'} LearnDeep 分类 chips (${categories}个)`)

    // 搜索功能
    const searchInput = page.locator('.search-input')
    if (await searchInput.count() > 0) {
      await searchInput.fill('redis')
      await page.waitForTimeout(800)
      const filteredCards = await page.locator('.article-card').count()
      console.log(`  ${filteredCards > 0 ? '✅' : '⚠️'} LearnDeep 搜索"redis" (${filteredCards}篇匹配)`)
      await searchInput.fill('')
    }

    // 进入详情
    if (articleCards > 0) {
      await page.locator('.article-card').first().click()
      await page.waitForTimeout(1500)
      const detailTitle = await page.locator('.detail-title').count()
      const markdownContent = await page.locator('.markdown-content').count()
      console.log(`  ${detailTitle > 0 ? '✅' : '❌'} LearnDeep 详情页 (标题:${detailTitle}, 内容:${markdownContent})`)

      // 返回
      const backBtn = page.locator('text=/返回/').first()
      if (await backBtn.count() > 0) {
        await backBtn.click()
        await page.waitForTimeout(500)
      }
    }
  } catch (e) {
    console.log(`  ❌ LearnDeep 失败: ${e.message}`)
  }

  console.log(`\n总览: ${pass} ✅  ${fail} ❌  (共 ${results.length} 页)`)

  if (errors.length > 0) {
    console.log('\n所有错误:')
    errors.slice(0, 10).forEach(e => console.log('  -', e.slice(0, 200)))
  }

  await browser.close()
  process.exit(fail > 0 ? 1 : 0)
})()
