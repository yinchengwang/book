/**
 * merge-and-check.js
 *
 * 合并首轮 241 项 + retry 27 项，找出仍缺失的项。
 *
 * 用法: node scripts/merge-and-check.js
 */

const fs = require('fs');
const path = require('path');

const dir = path.resolve(__dirname, 'data');

// 读取首轮结果
const firstPass = JSON.parse(fs.readFileSync(path.join(dir, 'generated-content.json'), 'utf-8'));
console.log('首轮:', firstPass.length, '项');

// 读取 retry 结果（从 workflow 输出）
// 我们有用 retry workflow 的输出文件路径
const retryOutputPath = process.argv[2];
if (!retryOutputPath) {
  console.error('用法: node merge-and-check.js <retry-output-json-path>');
  process.exit(1);
}
const retryOutput = JSON.parse(fs.readFileSync(retryOutputPath, 'utf-8'));
const retryContent = retryOutput.result.allContent;
console.log('Retry:', retryContent.length, '项');

// 合并，按 itemId 去重
const allMap = new Map();
for (const item of firstPass) {
  allMap.set(item.itemId, item);
}
for (const item of retryContent) {
  if (!allMap.has(item.itemId)) {
    allMap.set(item.itemId, item);
  } else {
    console.log('  去重跳过:', item.itemId, '(已在首轮中)');
  }
}

console.log('\n合并后:', allMap.size, '项\n');

// 读取 items-registry
const itemsPath = path.join(dir, 'items-registry.json');
const { items } = JSON.parse(fs.readFileSync(itemsPath, 'utf-8'));

const generatedIds = new Set(allMap.keys());
const missing = items.filter(item => !generatedIds.has(item.id));

if (missing.length === 0) {
  console.log('✅ 所有 300 项都已覆盖！');
  // 保存合并结果
  const mergedPath = path.join(dir, 'generated-content.json');
  fs.writeFileSync(mergedPath, JSON.stringify(Array.from(allMap.values()), null, 2), 'utf-8');
  console.log('已更新:', mergedPath);
  process.exit(0);
}

console.log(`❌ 仍缺失 ${missing.length} 项:\n`);

const byStack = {};
missing.forEach(item => {
  if (!byStack[item.stack]) byStack[item.stack] = [];
  byStack[item.stack].push(item);
});

Object.entries(byStack).forEach(([stack, misItems]) => {
  console.log(`\n--- ${stack} (${misItems.length} 项) ---`);
  misItems.forEach(item => {
    console.log(`  ${item.id.padEnd(35)} ${item.title}`);
  });
});

// 输出缺失 ID JSON
console.log('\n缺失 IDs:');
console.log(JSON.stringify(missing.map(i => i.id)));
