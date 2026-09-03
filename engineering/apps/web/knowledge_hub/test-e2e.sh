/**
 * 端到端测试主脚本
 * 跑法：bash test-e2e.sh
 *
 * 测什么：
 *  1. 安装/构建 H5 端
 *  2. 启动 H5 dev server
 *  3. 通过 Playwright 测所有页面
 *  4. 构建小程序端
 *  5. 报告总结
 */

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() { echo -e "${BLUE}[$(date +%H:%M:%S)]${NC} $1"; }
ok() { echo -e "${GREEN}✅ $1${NC}"; }
warn() { echo -e "${YELLOW}⚠️  $1${NC}"; }
err() { echo -e "${RED}❌ $1${NC}"; }

cd "$(dirname "$0")/.."
ROOT=$(pwd)

# ==================== 1. 环境检查 ====================
log "=== 1. 环境检查 ==="

if ! command -v node &> /dev/null; then
  err "node 未安装"
  exit 1
fi
NODE_VER=$(node --version)
ok "Node: $NODE_VER"

if ! command -v npm &> /dev/null; then
  err "npm 未安装"
  exit 1
fi
NPM_VER=$(npm --version)
ok "npm: $NPM_VER"

# ==================== 2. 安装依赖 ====================
log ""
log "=== 2. 安装依赖 ==="

# 顶层 Taro 工程
if [ ! -d "node_modules/@tarojs/cli" ]; then
  log "安装 Taro 依赖..."
  export NODE_OPTIONS=--openssl-legacy-provider
  npm install --include=dev --legacy-peer-deps 2>&1 | tail -3
  ok "Taro 依赖已装"
else
  ok "Taro 依赖已存在"
fi

# web/ 依赖
if [ ! -d "web/node_modules/vite" ]; then
  log "安装 web/ 依赖..."
  cd web
  npm install --legacy-peer-deps 2>&1 | tail -3
  cd ..
  ok "web 依赖已装"
else
  ok "web 依赖已存在"
fi

# ==================== 3. 启动 H5 dev server ====================
log ""
log "=== 3. 启动 H5 dev server ==="

cd web
# 启动 dev server 后台
nohup npm run dev > /tmp/h5-dev.log 2>&1 &
H5_PID=$!
cd ..

# 等待启动
log "等待 dev server 启动..."
for i in {1..30}; do
  if curl -s http://localhost:5173 > /dev/null 2>&1; then
    ok "H5 dev server 启动成功 (PID: $H5_PID)"
    break
  fi
  sleep 1
done

if ! curl -s http://localhost:5173 > /dev/null 2>&1; then
  err "H5 dev server 启动失败，查看日志:"
  cat /tmp/h5-dev.log | tail -20
  kill $H5_PID 2>/dev/null
  exit 1
fi

# ==================== 4. Playwright 测所有页面 ====================
log ""
log "=== 4. Playwright 测试所有页面 ==="

mkdir -p test-e2e
cat > /tmp/test-pages.js <<'EOF'
const { chromium } = require('playwright')

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

;(async () => {
  const browser = await chromium.launch()
  const page = await browser.newPage()
  const errors = []
  page.on('pageerror', e => errors.push(`PAGE_ERROR: ${e.message}`))
  page.on('console', m => {
    if (m.type() === 'error') errors.push(`CONSOLE: ${m.text()}`)
  })

  let pass = 0, fail = 0
  const results = []

  for (const p of PAGES) {
    errors.length = 0
    try {
      await page.goto(`http://localhost:5173${p.path}`, { waitUntil: 'networkidle', timeout: 15000 })
      await page.waitForTimeout(2000)
      const mainText = await page.evaluate(() => {
        const main = document.querySelector('.page-container')
        return main ? main.innerText.trim() : ''
      })
      const hasContent = mainText.length > 20
      const hasError = errors.some(e => e.includes('TypeError') || e.includes('SyntaxError'))
      const status = hasError ? '❌' : (hasContent ? '✅' : '⚠️')
      if (status === '✅') pass++
      else fail++
      results.push({ name: p.name, status, len: mainText.length, errors: errors.slice(0, 2) })
      console.log(`  ${status} ${p.name} (${mainText.length}字) ${hasError ? errors[0] : ''}`)
    } catch (e) {
      fail++
      results.push({ name: p.name, status: '❌', error: e.message })
      console.log(`  ❌ ${p.name} - ${e.message}`)
    }
  }

  console.log(`\n总览: ${pass} ✅  ${fail} ❌`)

  // 测试关键交互
  console.log('\n--- 关键交互测试 ---')
  errors.length = 0

  // Quiz: 点选项
  await page.goto('http://localhost:5173/quiz', { waitUntil: 'networkidle' })
  await page.waitForTimeout(1500)
  const quizOpt = await page.locator('.page-container [class*="option"]').first()
  if (await quizOpt.count() > 0) {
    await quizOpt.click()
    await page.waitForTimeout(500)
    console.log('  ✅ Quiz 选项点击')
  }

  // Settings: 切换开关
  await page.goto('http://localhost:5173/settings', { waitUntil: 'networkidle' })
  await page.waitForTimeout(1500)
  const sw = await page.locator('.page-container [role="switch"]').first()
  if (await sw.count() > 0) {
    const before = await sw.getAttribute('aria-checked')
    await sw.click()
    await page.waitForTimeout(300)
    const after = await sw.getAttribute('aria-checked')
    console.log(`  ${before !== after ? '✅' : '⚠️'} Settings 开关切换 (${before}→${after})`)
  }

  await browser.close()
  process.exit(fail > 0 ? 1 : 0)
})()
EOF

# 检查 playwright
if [ ! -d "web/node_modules/playwright" ]; then
  log "安装 playwright..."
  cd web
  npm install --save-dev playwright --legacy-peer-deps 2>&1 | tail -3
  cd ..
fi

cd web
node /tmp/test-pages.js
TEST_EXIT=$?
cd ..

# ==================== 5. 关闭 dev server ====================
log ""
log "=== 5. 关闭 dev server ==="
kill $H5_PID 2>/dev/null && ok "dev server 已关闭" || warn "dev server 未运行"

# ==================== 6. 构建小程序端 ====================
log ""
log "=== 6. 构建小程序端 ==="

export NODE_OPTIONS=--openssl-legacy-provider
if npm run build:weapp 2>&1 | tail -20; then
  ok "小程序构建成功"
  WEAPP_OK=1
else
  err "小程序构建失败"
  WEAPP_OK=0
fi

# ==================== 7. 总结 ====================
log ""
log "=========================================="
log "=== 测试总结 ==="
log "=========================================="

if [ $TEST_EXIT -eq 0 ] && [ $WEAPP_OK -eq 1 ]; then
  ok "全部通过！H5 端和小程序端都正常"
  exit 0
else
  err "有测试失败，详情查看上方日志"
  exit 1
fi
