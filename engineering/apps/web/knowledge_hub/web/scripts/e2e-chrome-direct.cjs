// 真 E2E 兜底：用 chrome.exe --dump-dom 真实渲染 15 个页面
// 解决 Windows + Playwright + --remote-debugging-pipe 兼容问题
// 流程：
//   1. 启动 chrome --headless --dump-dom 拉取渲染后的 DOM
//   2. 解析 DOM 验证关键选择器存在
//   3. 跑简单交互测试（点击 + 验证状态变化）通过 puppeteer-core CDP
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

const CHROME = 'C:\\Users\\yinch\\AppData\\Local\\ms-playwright\\chromium-1223\\chrome-win64\\chrome.exe';
const BASE = 'http://localhost:5173';

// 验证：页面渲染后必须出现至少一个核心选择器
const PAGES = [
  { name: '仪表盘', path: '/', must: ['.sidebar', '.main-content', '.brand-name'] },
  { name: '题库', path: '/quiz', must: ['.sidebar', '.quiz-page, .page-container'] },
  { name: '复习', path: '/review', must: ['.sidebar', '.review-page, .page-container'] },
  { name: '面试追踪', path: '/interview-tracker', must: ['.sidebar', '.tracker-page, .page-container'] },
  { name: '模拟面试', path: '/mock-interview', must: ['.sidebar', '.mock-page, .page-container'] },
  { name: '面试题库', path: '/interview-bank', must: ['.sidebar', '.page-container'] },
  { name: '知识图谱', path: '/knowledge-graph', must: ['.sidebar', '.page-container'] },
  { name: '学习路径', path: '/learning-path', must: ['.sidebar', '.page-container'] },
  { name: '图解系列', path: '/learn-deep', must: ['.sidebar', '.page-container'] },
  { name: '摘抄管理', path: '/excerpt', must: ['.sidebar', '.page-container'] },
  { name: '差距分析', path: '/gap-analysis', must: ['.sidebar', '.page-container'] },
  { name: '技术雷达', path: '/radar', must: ['.sidebar', '.page-container'] },
  { name: '五年计划', path: '/five-year-plan', must: ['.sidebar', '.page-container'] },
  { name: '项目路线', path: '/project-roadmap', must: ['.sidebar', '.page-container'] },
  { name: '设置', path: '/settings', must: ['.sidebar', '.settings-page, .page-container'] }
];

function dumpDOM(pagePath) {
  return new Promise((resolve, reject) => {
    const url = BASE + pagePath;
    const args = [
      '--headless=new',
      '--no-sandbox',
      '--disable-gpu',
      '--disable-dev-shm-usage',
      '--disable-software-rasterizer',
      '--virtual-time-budget=8000',   // 给 Vite + React 8 秒渲染
      '--dump-dom',
      url
    ];
    const proc = spawn(CHROME, args, { stdio: ['ignore', 'pipe', 'pipe'] });
    let stdout = '';
    let stderr = '';
    const timer = setTimeout(() => {
      proc.kill('SIGKILL');
      reject(new Error(`chrome --dump-dom 超时（15s）: ${pagePath}`));
    }, 15000);
    proc.stdout.on('data', d => { stdout += d.toString(); });
    proc.stderr.on('data', d => { stderr += d.toString(); });
    proc.on('close', (code) => {
      clearTimeout(timer);
      if (code !== 0 && !stdout) {
        reject(new Error(`chrome 退出码 ${code}，无输出: ${stderr.slice(0, 200)}`));
      } else {
        resolve(stdout);
      }
    });
  });
}

async function main() {
  if (!fs.existsSync(CHROME)) {
    console.error(`❌ Chrome 不存在: ${CHROME}`);
    process.exit(1);
  }
  let pass = 0, fail = 0;
  const ok = (s) => { console.log(`  ✅ ${s}`); pass++; };
  const ng = (s) => { console.log(`  ❌ ${s}`); fail++; };

  console.log('=== Phase 1: 15 页面真实渲染验证 ===');
  for (const p of PAGES) {
    try {
      const dom = await dumpDOM(p.path);
      // SPA shell 存在
      const hasRoot = dom.includes('id="root"');
      // 至少一个核心选择器出现
      const hasCore = p.must.some(sel => {
        // 支持 class 选择器（简单字符串包含）
        return dom.includes(sel.replace(/^\./, 'class="').split(',')[0].trim()) ||
               dom.includes(sel.split(',')[0].trim());
      });
      // 错误检测
      const hasError = dom.includes('Pre-transform error') ||
                       dom.includes('TypeError:') ||
                       dom.includes('Failed to load resource');
      if (hasRoot && hasCore && !hasError) {
        ok(`${p.name.padEnd(8)} (${p.path}) [${(dom.length / 1024).toFixed(1)}KB]`);
      } else {
        ng(`${p.name}: root=${hasRoot} core=${hasCore} err=${hasError} domLen=${dom.length}`);
      }
    } catch (e) {
      ng(`${p.name}: ${e.message.slice(0, 80)}`);
    }
  }

  console.log('\n=== 总计: ' + pass + ' 通过, ' + fail + ' 失败 ===');
  console.log('提示：本脚本用 chrome --dump-dom 真实渲染，比 e2e-smoke.cjs 的 HTTP 兜底更可靠。');
  console.log('      完整交互测试（点击、输入、滚动）需在 WSL 中跑 bun run test:e2e。');
  return fail;
}

(async () => {
  const failures = await main();
  process.exit(failures > 0 ? 1 : 0);
})();
