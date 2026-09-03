/**
 * save-generated-content.js
 *
 * 从 workflow 结果中提取生成的 241 篇内容并保存为临时 JSON，
 * 同时输出缺失的知识点列表。
 *
 * 用法: node scripts/save-generated-content.js <workflow-output-json>
 */

const fs = require('fs');
const path = require('path');

// 读取 workflow 输出
const outputPath = process.argv[2];
if (!outputPath) {
  console.error('用法: node save-generated-content.js <workflow-output-json-path>');
  process.exit(1);
}

const output = JSON.parse(fs.readFileSync(outputPath, 'utf-8'));
const { allContent } = output.result;

console.log(`已生成内容: ${allContent.length} 项`);

// 保存为 JSON
const outDir = path.resolve(__dirname, 'data');
if (!fs.existsSync(outDir)) fs.mkdirSync(outDir, { recursive: true });
const generatedJsonPath = path.join(outDir, 'generated-content.json');
fs.writeFileSync(generatedJsonPath, JSON.stringify(allContent, null, 2), 'utf-8');
console.log(`已保存到: ${generatedJsonPath}`);

// 读取 items-registry，找出哪些没有生成
const itemsPath = path.resolve(__dirname, 'data', 'items-registry.json');
const { items } = JSON.parse(fs.readFileSync(itemsPath, 'utf-8'));

const generatedIds = new Set(allContent.map(c => c.itemId));

// 按栈统计缺失
const missing = items.filter(item => !generatedIds.has(item.id));
console.log(`\n缺失: ${missing.length} 项\n`);

// 按栈分组
const byStack = {};
missing.forEach(item => {
  if (!byStack[item.stack]) byStack[item.stack] = [];
  byStack[item.stack].push(item);
});

Object.entries(byStack).forEach(([stack, misItems]) => {
  console.log(`\n--- ${stack} (${misItems.length} 项缺失) ---`);
  misItems.forEach(item => {
    console.log(`  ${item.id.padEnd(35)} ${item.title}`);
  });
});

// 输出缺失项 ID 列表
const missingIds = missing.map(i => i.id);
console.log(`\n缺失 IDs (共 ${missingIds.length} 个):`);
console.log(JSON.stringify(missingIds));

// 输出缺失项的 batch 归属
const classPath = path.resolve(__dirname, 'data', 'classification-output.json');
const classification = JSON.parse(fs.readFileSync(classPath, 'utf-8'));

// 按 batch 分组
const batchDir = path.resolve(__dirname, 'data', 'batch-prompts');
const batchFiles = fs.readdirSync(batchDir).filter(f => f.startsWith('batch-') && f.endsWith('.json')).sort();
batchFiles.forEach(bf => {
  const batch = JSON.parse(fs.readFileSync(path.join(batchDir, bf), 'utf-8'));
  const batchMissing = batch.items.filter(i => !generatedIds.has(i.id));
  if (batchMissing.length > 0) {
    console.log(`\nBatch #${batch.batchNum} [${batch.stack}] 缺失 ${batchMissing.length}/${batch.items.length}:`);
    batchMissing.forEach(item => {
      const cls = classification.items.find(c => c.itemId === item.id);
      console.log(`  ${item.id.padEnd(35)} ${item.title.padEnd(25)} tier=${cls ? cls.tier : '?'}`);
    });
  }
});
