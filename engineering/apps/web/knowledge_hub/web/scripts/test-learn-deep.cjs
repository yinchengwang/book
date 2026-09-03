/**
 * 图解系列双栏布局自动化测试
 *
 * 验证：
 * 1. 双栏布局（侧边栏 + 内容区）
 * 2. 分类导航和折叠展开
 * 3. 文章选择和显示
 * 4. 上/下篇导航
 * 5. 搜索功能
 */
const { chromium } = require('playwright')

;(async () => {
  console.log('🚀 启动图解系列功能验证...\n')
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

  const ctx = await browser.newContext({ viewport: { width: 1280, height: 800 } })
  const page = await ctx.newPage()
  const errors = []
  page.on('pageerror', e => errors.push(`PAGE: ${e.message}`))
  page.on('console', m => {
    if (m.type() === 'error' && !m.text().includes('legacy-js-api')) {
      errors.push(`CONSOLE: ${m.text().slice(0, 150)}`)
    }
  })

  let pass = 0, fail = 0

  // ========== 测试图解系列 ==========
  console.log('--- 图解系列功能验证 ---\n')

  // 1. 页面加载
  console.log('1️⃣ 页面加载...')
  try {
    await page.goto('http://localhost:5173/learn_deep', { waitUntil: 'networkidle', timeout: 30000 })
    await page.waitForTimeout(2000)

    // 检查双栏布局
    const layout = await page.locator('.learn-deep-layout').count()
    const header = await page.locator('.layout-header').count()
    const body = await page.locator('.layout-body').count()
    const sidebar = await page.locator('.category-sidebar').count()
    const content = await page.locator('.layout-content').count()

    console.log(`  ${layout > 0 ? '✅' : '❌'} 双栏布局容器 .learn-deep-layout (${layout})`)
    console.log(`  ${header > 0 ? '✅' : '❌'} 顶部栏 .layout-header (${header})`)
    console.log(`  ${body > 0 ? '✅' : '❌'} 主体 .layout-body (${body})`)
    console.log(`  ${sidebar > 0 ? '✅' : '❌'} 侧边栏 .category-sidebar (${sidebar})`)
    console.log(`  ${content > 0 ? '✅' : '❌'} 内容区 .layout-content (${content})`)

    if (layout > 0 && header > 0 && body > 0 && sidebar > 0 && content > 0) {
      pass++
    } else {
      fail++
    }
  } catch (e) {
    console.log(`  ❌ 页面加载失败: ${e.message}`)
    fail++
  }

  // 2. 分类导航
  console.log('\n2️⃣ 分类导航...')
  try {
    // 检查 9 个分类
    const categories = await page.locator('.category-group').count()
    console.log(`  ${categories >= 9 ? '✅' : '⚠️'} 分类数量 (${categories}/9)`)

    // 检查分类名称
    const categoryNames = await page.locator('.category-name').allTextContents()
    console.log(`  📋 分类列表: ${categoryNames.slice(0, 5).join(', ')}...`)

    // 展开第一个分类
    await page.locator('.category-group').first().click()
    await page.waitForTimeout(500)

    // 检查文章列表
    const articles = await page.locator('.article-item').count()
    console.log(`  ${articles > 0 ? '✅' : '❌'} 展开后显示文章列表 (${articles}篇)`)

    if (categories >= 9 && articles > 0) pass++
    else fail++
  } catch (e) {
    console.log(`  ❌ 分类导航失败: ${e.message}`)
    fail++
  }

  // 3. 搜索功能
  console.log('\n3️⃣ 搜索功能...')
  try {
    const searchInput = page.locator('.search-bar .search-input')
    if (await searchInput.count() > 0) {
      await searchInput.fill('redis')
      await page.waitForTimeout(800)
      const filteredArticles = await page.locator('.article-item').count()
      console.log(`  ${filteredArticles > 0 ? '✅' : '❌'} 搜索"redis"结果 (${filteredArticles}篇)`)

      // 清除搜索
      const clearBtn = page.locator('.search-clear')
      if (await clearBtn.count() > 0) {
        await clearBtn.click()
        await page.waitForTimeout(500)
        const restoredArticles = await page.locator('.article-item').count()
        console.log(`  ${restoredArticles > 0 ? '✅' : '❌'} 清除搜索恢复列表 (${restoredArticles}篇)`)
      }

      if (filteredArticles > 0) pass++
      else fail++
    } else {
      console.log('  ⚠️ 未找到搜索框')
      fail++
    }
  } catch (e) {
    console.log(`  ❌ 搜索功能失败: ${e.message}`)
    fail++
  }

  // 4. 文章选择和显示
  console.log('\n4️⃣ 文章选择和显示...')
  try {
    // 点击第一篇文章
    const firstArticle = page.locator('.article-item').first()
    if (await firstArticle.count() > 0) {
      await firstArticle.click()
      await page.waitForTimeout(1500)

      // 检查阅读器
      const reader = await page.locator('.article-reader').count()
      const readerHeader = await page.locator('.reader-header').count()
      const readerTitle = await page.locator('.reader-title').count()
      const readerContent = await page.locator('.reader-content').count()
      const readerFooter = await page.locator('.reader-footer').count()

      console.log(`  ${reader > 0 ? '✅' : '❌'} 阅读器 .article-reader (${reader})`)
      console.log(`  ${readerHeader > 0 ? '✅' : '❌'} 阅读器头部 .reader-header (${readerHeader})`)
      console.log(`  ${readerTitle > 0 ? '✅' : '❌'} 文章标题 .reader-title (${readerTitle})`)
      console.log(`  ${readerContent > 0 ? '✅' : '❌'} 文章内容 .reader-content (${readerContent})`)
      console.log(`  ${readerFooter > 0 ? '✅' : '❌'} 阅读器底部 .reader-footer (${readerFooter})`)

      // 检查面包屑
      const breadcrumb = await page.locator('.reader-breadcrumb').count()
      console.log(`  ${breadcrumb > 0 ? '✅' : '❌'} 面包屑导航 .reader-breadcrumb (${breadcrumb})`)

      if (reader > 0 && readerTitle > 0 && readerContent > 0) pass++
      else fail++
    } else {
      console.log('  ⚠️ 未找到文章项')
      fail++
    }
  } catch (e) {
    console.log(`  ❌ 文章选择失败: ${e.message}`)
    fail++
  }

  // 5. 上/下篇导航
  console.log('\n5️⃣ 上/下篇导航...')
  try {
    // 检查导航卡片
    const navCards = await page.locator('.nav-card').count()
    const prevCard = await page.locator('.nav-prev').count()
    const nextCard = await page.locator('.nav-next').count()
    console.log(`  ${navCards >= 2 ? '✅' : '❌'} 导航卡片 (${navCards}/2)`)
    console.log(`  ${prevCard > 0 ? '✅' : '❌'} 上一篇卡片 .nav-prev (${prevCard})`)
    console.log(`  ${nextCard > 0 ? '✅' : '❌'} 下一篇卡片 .nav-next (${nextCard})`)

    // 获取当前文章标题
    const currentTitle = await page.locator('.reader-title').textContent()

    // 点击下一篇
    const nextBtn = page.locator('.nav-next:not(.nav-disabled)')
    if (await nextBtn.count() > 0) {
      await nextBtn.click()
      await page.waitForTimeout(1500)
      const newTitle = await page.locator('.reader-title').textContent()
      console.log(`  ${currentTitle !== newTitle ? '✅' : '❌'} 点击下一篇切换文章`)
      console.log(`     "${currentTitle}" → "${newTitle}"`)
    } else {
      console.log('  ⚠️ 已经是最后一篇')
    }

    if (navCards >= 2) pass++
    else fail++
  } catch (e) {
    console.log(`  ❌ 上/下篇导航失败: ${e.message}`)
    fail++
  }

  // 6. 返回列表
  console.log('\n6️⃣ 返回列表...')
  try {
    const backLink = page.locator('.breadcrumb-link')
    if (await backLink.count() > 0) {
      await backLink.click()
      await page.waitForTimeout(1000)
      const placeholder = await page.locator('.content-placeholder').count()
      console.log(`  ${placeholder > 0 ? '✅' : '❌'} 返回后显示占位符`)
      if (placeholder > 0) pass++
      else fail++
    } else {
      console.log('  ⚠️ 未找到返回链接')
      fail++
    }
  } catch (e) {
    console.log(`  ❌ 返回列表失败: ${e.message}`)
    fail++
  }

  // 7. 错误检查
  console.log('\n7️⃣ 错误检查...')
  const pageErrors = errors.filter(e => e.startsWith('PAGE:') || e.startsWith('CONSOLE:'))
  if (pageErrors.length === 0) {
    console.log('  ✅ 无控制台错误')
    pass++
  } else {
    console.log(`  ❌ 发现 ${pageErrors.length} 个错误:`)
    pageErrors.slice(0, 5).forEach(e => console.log(`     - ${e.slice(0, 100)}`))
    fail++
  }

  // ========== 结果汇总 ==========
  console.log('\n' + '='.repeat(50))
  console.log(`📊 测试结果: ${pass} 通过 / ${fail} 失败`)
  console.log('='.repeat(50))

  if (errors.length > 0) {
    console.log('\n⚠️ 控制台消息:')
    errors.slice(0, 10).forEach(e => console.log('  -', e.slice(0, 200)))
  }

  await browser.close()
  process.exit(fail > 0 ? 1 : 0)
})()