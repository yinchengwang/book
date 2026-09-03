// 简版 E2E 测试：HTTP 兜底
// Windows 平台 Chromium --remote-debugging-pipe 兼容性问题，直接走 HTTP 验证
// 验证：
//   1. 15 个页面 HTTP 200 + SPA shell
//   2. 18 关键模块文件能被 Vite 转译（无编译错误）
//   3. 5 改过的核心文件（store + 页面）无 Pre-transform error
const { chromium } = require('playwright');

const HEADLESS_SHELL = 'C:\\Users\\yinch\\AppData\\Local\\ms-playwright\\chromium_headless_shell-1223\\chrome-headless-shell-win64\\chrome-headless-shell.exe';

const PAGES = [
  { name: '仪表盘', path: '/' },
  { name: '题库', path: '/quiz' },
  { name: '复习', path: '/review' },
  { name: '面试追踪', path: '/interview-tracker' },
  { name: '模拟面试', path: '/mock-interview' },
  { name: '面试题库', path: '/interview-bank' },
  { name: '知识图谱', path: '/knowledge-graph' },
  { name: '学习路径', path: '/learning-path' },
  { name: '图解系列', path: '/learn-deep' },
  { name: '摘抄管理', path: '/excerpt' },
  { name: '差距分析', path: '/gap-analysis' },
  { name: '技术雷达', path: '/radar' },
  { name: '五年计划', path: '/five-year-plan' },
  { name: '项目路线', path: '/project-roadmap' },
  { name: '设置', path: '/settings' }
];

const MODULES = [
  '/src/pages/learning-path/LearningPath.tsx',
  '/src/pages/excerpt/Excerpt.tsx',
  '/src/pages/review/Review.tsx',
  '/src/pages/quiz/Quiz.tsx',
  '/src/pages/index/index.tsx',
  '/src/pages/learn-deep/LearnDeep.tsx',
  '/src/pages/radar/Radar.tsx',
  '/src/pages/gap-analysis/GapAnalysis.tsx',
  '/src/pages/knowledge-graph/KnowledgeGraph.tsx',
  '/src/stores/learningPathStore.ts',
  '/src/stores/excerptStore.ts',
  '/src/pages/settings/Settings.tsx',
  '/src/pages/interview-tracker/InterviewTracker.tsx',
  '/src/pages/mock-interview/MockInterview.tsx',
  '/src/pages/interview-bank/InterviewBank.tsx',
  '/web/styles/light-fixes.scss',
  '/web/styles/global.scss',
  '/web/main.tsx'
];

const CORE_MODIFIED = [
  '/src/pages/learning-path/LearningPath.tsx',
  '/src/pages/excerpt/Excerpt.tsx',
  '/src/pages/quiz/Quiz.tsx',
  '/src/stores/learningPathStore.ts',
  '/src/stores/excerptStore.ts'
];

async function main() {
  let pass = 0, fail = 0;
  const ok = (s) => { console.log(`  ✅ ${s}`); pass++; };
  const ng = (s) => { console.log(`  ❌ ${s}`); fail++; };

  console.log('=== Phase 1: 15 页 HTTP 加载 ===');
  for (const p of PAGES) {
    try {
      const res = await fetch(`http://localhost:5173${p.path}`);
      const html = await res.text();
      const hasRoot = html.includes('id="root"');
      const hasVite = html.includes('/@vite/client');
      if (res.status === 200 && hasRoot && hasVite) {
        ok(`${p.name.padEnd(8)} (${p.path})`);
      } else {
        ng(`${p.name}: status=${res.status} root=${hasRoot} vite=${hasVite}`);
      }
    } catch (e) {
      ng(`${p.name}: ${e.message.slice(0, 80)}`);
    }
  }

  console.log('\n=== Phase 2: 18 关键模块加载 ===');
  for (const m of MODULES) {
    try {
      const res = await fetch(`http://localhost:5173${m}`);
      const body = await res.text();
      if (res.status === 200 && body.length > 100) {
        const short = m.replace('/src/', '').replace('/web/', 'web/');
        ok(short);
      } else {
        ng(`${m}: status=${res.status} bodyLen=${body.length}`);
      }
    } catch (e) {
      ng(`${m}: ${e.message.slice(0, 60)}`);
    }
  }

  console.log('\n=== Phase 3: 5 改过的核心文件编译错误扫描 ===');
  let compileErrorCount = 0;
  for (const m of CORE_MODIFIED) {
    try {
      const res = await fetch(`http://localhost:5173${m}`);
      const body = await res.text();
      const hasError = body.includes('Pre-transform error') ||
                       body.includes('TypeError:') ||
                       body.includes('TS2') ||
                       body.includes('Failed to');
      if (hasError) {
        ng(`${m}: 编译错误`);
        console.log(`     ${body.slice(0, 300)}`);
        compileErrorCount++;
      } else {
        ok(m.replace('/src/', ''));
      }
    } catch (e) {
      ng(`${m}: ${e.message.slice(0, 60)}`);
    }
  }
  if (compileErrorCount === 0) {
    console.log('  → 5 个核心文件全部无编译错误 ✅');
  }

  console.log(`\n=== 总计: ${pass} 通过, ${fail} 失败 ===`);
  console.log('提示：Windows 上 Chromium 启动超时，建议在 WSL 中跑完整 Playwright e2e');
  return fail;
}

(async () => {
  const failures = await main();
  process.exit(failures > 0 ? 1 : 0);
})();
