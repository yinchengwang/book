/**
 * prepare-workflow-args.js
 *
 * 读取所有 batch-XX.json 文件，打包成 Workflow 脚本可用的 args 数据。
 * 直接输出文件供 Workflow 使用。
 *
 * 用法: node scripts/prepare-workflow-args.js
 * 输出: scripts/data/workflow-batches.json
 */

const fs = require('fs');
const path = require('path');

const promtsDir = path.resolve(__dirname, 'data', 'batch-prompts');
const outPath = path.resolve(__dirname, 'data', 'workflow-batches.json');

const batchFiles = fs.readdirSync(promtsDir)
  .filter(f => f.startsWith('batch-') && f.endsWith('.json'))
  .sort();

const batches = batchFiles.map(f => {
  return JSON.parse(fs.readFileSync(path.join(promtsDir, f), 'utf-8'));
});

fs.writeFileSync(outPath, JSON.stringify(batches, null, 2), 'utf-8');
console.log(`已打包 ${batches.length} 批数据到 workflow-batches.json`);
console.log(`总 items: ${batches.reduce((s, b) => s + b.items.length, 0)}`);
