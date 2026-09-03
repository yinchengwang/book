/**
 * verify.js
 *
 * Phase 6 验证脚本
 * 检查：覆盖、bundle 语法、内容质量、git 状态
 */
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const MD_DIR = path.join(ROOT, 'data', 'learn-deep');
const BUNDLE_PATH = path.join(ROOT, 'data', 'app', 'learn-deep-bundle.js');

const items = JSON.parse(fs.readFileSync(path.resolve(__dirname, 'data', 'items-registry.json'), 'utf-8')).items;

console.log('=== 1. 覆盖检查 ===');
const mdFiles = fs.readdirSync(MD_DIR).filter(f => f.endsWith('.md'));
const mdIds = new Set(mdFiles.map(f => f.replace(/\.md$/, '')));
const bundle = fs.readFileSync(BUNDLE_PATH, 'utf-8');
const bundleIds = (bundle.match(/"([\w-]+)":/g) || []).map(s => s.replace(/[":]/g, ''));
const bundleSet = new Set(bundleIds);

let missingMd = 0, missingBundle = 0;
for (const item of items) {
  if (!mdIds.has(item.id)) { console.log('  ❌ MISSING .md:', item.id); missingMd++; }
  if (!bundleSet.has(item.id)) { console.log('  ❌ MISSING bundle:', item.id); missingBundle++; }
}
if (missingMd === 0) console.log('  ✅ 300/300 .md files present');
if (missingBundle === 0) console.log('  ✅ 300/300 bundle entries present');

console.log('\n=== 2. Bundle JS 语法检查 ===');
try {
  const jsonPart = bundle.replace('"use strict";window.LEARN_DEEP_CONTENT = ', '').replace(/;\s*$/, '');
  JSON.parse(jsonPart);
  console.log('  ✅ Bundle JSON 解析正确');
} catch (e) {
  console.log('  ❌ Bundle 解析错误:', e.message.slice(0, 200));
}

console.log('\n=== 3. 文件大小统计 ===');
let totalMdSize = 0;
for (const f of mdFiles) {
  totalMdSize += fs.statSync(path.join(MD_DIR, f)).size;
}
console.log('  .md 文件总计:', mdFiles.length, '个,', (totalMdSize / 1024 / 1024).toFixed(1), 'MB');
console.log('  Bundle:', (fs.statSync(BUNDLE_PATH).size / 1024 / 1024).toFixed(1), 'MB');

console.log('\n=== 4. 抽查内容质量 ===');
const samples = ['pointer', 'db-hnsw', 'ds-hashtable', 'linux-ebpf-intro', 'py-numpy', 'raii'];
for (const id of samples) {
  const fp = path.join(MD_DIR, id + '.md');
  if (!fs.existsSync(fp)) { console.log('  ❌', id, '文件缺失'); continue; }
  const content = fs.readFileSync(fp, 'utf-8');
  const lines = content.split('\n').length;
  const firstH1 = content.match(/^# .+/m);
  const hasCode = content.includes('```');
  console.log('  ' + id + ': ' + lines + '行, ' + (firstH1 ? firstH1[0].trim() : '无标题') + (hasCode ? ', 💻有代码' : ''));
}

console.log('\n=== 5. 栈分布 ===');
const byStack = {};
for (const item of items) {
  if (!byStack[item.stack]) byStack[item.stack] = { count: 0, mdSize: 0 };
  byStack[item.stack].count++;
  const fp = path.join(MD_DIR, item.id + '.md');
  if (fs.existsSync(fp)) byStack[item.stack].mdSize += fs.statSync(fp).size;
}
for (const [stack, info] of Object.entries(byStack)) {
  console.log('  ' + stack.padEnd(6) + ': ' + info.count + '项, ' + (info.mdSize / 1024).toFixed(0) + ' KB');
}

console.log('\n=== 6. Git 状态 ===');
const { execSync } = require('child_process');
try {
  const status = execSync('git -C "' + ROOT + '" status --short', { encoding: 'utf-8' });
  const lines = status.trim().split('\n').filter(l => l.trim());
  console.log('  变更文件:', lines.length);
  // Show summary
  const modified = lines.filter(l => l.startsWith(' M') || l.startsWith('M ')).length;
  const added = lines.filter(l => l.startsWith('??') || l.startsWith('A ')).length;
  const deleted = lines.filter(l => l.startsWith(' D') || l.startsWith('D ')).length;
  console.log('  修改:', modified, ' 新增:', added, ' 删除:', deleted);
} catch (e) {
  console.log('  Error:', e.message);
}

console.log('\n✅ 验证完成');
