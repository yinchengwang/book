/**
 * collect-all-content.js
 *
 * 收集所有已生成的内容：
 * 1. 读取首轮生成的 generated-content.json (267 项)
 * 2. 读取 retry workflow 输出中返回的内容
 * 3. 扫描 data/learn-deep/ 目录下所有 .md 文件中 agent 可能直接写入的内容
 * 4. 读取 items-registry 找出仍缺失的
 * 5. 输出最终合并结果
 *
 * 用法: node scripts/collect-all-content.js <retry-output-path>
 */

const fs = require('fs');
const path = require('path');

const dir = path.resolve(__dirname, 'data');
const learnDeepDir = path.resolve(__dirname, '..', 'data', 'learn-deep');

// 1. 读取首轮生成内容
const firstPass = JSON.parse(fs.readFileSync(path.join(dir, 'generated-content.json'), 'utf-8'));
console.log('首轮:', firstPass.length, '项');

// 2. 读取 retry 输出中的内容
const retryOutputPath = process.argv[2];
let retryItems = [];
if (retryOutputPath) {
  const raw = JSON.parse(fs.readFileSync(retryOutputPath, 'utf-8'));
  retryItems = raw.result.allContent || [];
}
console.log('Retry 返回:', retryItems.length, '项');

// 3. 扫描 learn-deep 目录所有 .md 文件
function scanMdFiles(dirPath) {
  const results = [];
  try {
    const entries = fs.readdirSync(dirPath, { withFileTypes: true });
    for (const entry of entries) {
      const fullPath = path.join(dirPath, entry.name);
      if (entry.isDirectory()) {
        results.push(...scanMdFiles(fullPath));
      } else if (entry.name.endsWith('.md')) {
        const id = entry.name.replace(/\.md$/, '');
        results.push({ id, filePath: fullPath, source: 'learn-deep' });
      }
    }
  } catch (e) { /* dir may not exist */ }
  return results;
}

const mdFiles = scanMdFiles(learnDeepDir);
console.log('Learn-deep 目录 .md 文件:', mdFiles.length, '个');

// 4. 合并去重
const allMap = new Map();

for (const item of firstPass) {
  allMap.set(item.itemId, item);
}

let retryCount = 0;
for (const item of retryItems) {
  const key = item.itemId;
  if (!allMap.has(key)) {
    // Try to extract meaningful content from agent message
    let content = item.content || '';
    // The agent might have written file path instead of content
    // Try to read the file it mentioned
    const fileMatch = content.match(/[`']?([a-zA-Z]:\\[^`'"\n]+\.md)[`']?/);
    if (fileMatch) {
      try {
        const fileContent = fs.readFileSync(fileMatch[1], 'utf-8');
        content = fileContent;
        console.log('  从文件读取:', key, fileMatch[1]);
      } catch (e) {
        // file may not exist
      }
    }
    allMap.set(key, { itemId: key, content, batchNum: 999, stack: item.stack });
    retryCount++;
  }
}
console.log('Retry 新增 (含文件读取):', retryCount, '项');
console.log('合并后总计:', allMap.size, '项');

// 5. 检查缺失
const itemsPath = path.join(dir, 'items-registry.json');
const { items } = JSON.parse(fs.readFileSync(itemsPath, 'utf-8'));

const generatedIds = new Set(allMap.keys());
// Also check .md files that exist
for (const mf of mdFiles) {
  generatedIds.add(mf.id);
}

const missing = items.filter(item => !generatedIds.has(item.id));
const covered = items.filter(item => generatedIds.has(item.id));

console.log(`\n覆盖: ${covered.length}/300`);
console.log(`缺失: ${missing.length}/300\n`);

if (missing.length > 0) {
  const byStack = {};
  missing.forEach(item => {
    if (!byStack[item.stack]) byStack[item.stack] = [];
    byStack[item.stack].push(item);
  });
  Object.entries(byStack).forEach(([stack, misItems]) => {
    console.log(`\n--- ${stack} (${misItems.length} 项) ---`);
    misItems.forEach(item => console.log(`  ${item.id.padEnd(35)} ${item.title}`));
  });
  console.log('\n缺失 IDs:', JSON.stringify(missing.map(i => i.id)));
} else {
  console.log('✅ 全部覆盖！');
}

// 6. 检查 learn-deep 目录下哪些文章有内容但不在 allMap 中
const mdOnly = mdFiles.filter(mf => !allMap.has(mf.id));
if (mdOnly.length > 0) {
  console.log(`\n仅在 learn-deep 目录中有文件但不在合并结果中的: ${mdOnly.length} 项`);
  mdOnly.forEach(mf => console.log(`  ${mf.id.padEnd(35)} ${mf.filePath}`));
}

// 7. 检查 allMap 中有但 learn-deep 目录没有文件的
const noMdFile = items.filter(item => allMap.has(item.id) && !mdFiles.some(mf => mf.id === item.id));
console.log(`\n有内容但 learn-deep 目录无 .md 文件: ${noMdFile.length} 项`);
// just show count

// 8. 保存合并后的完整内容（只保存有实际内容的）
const validContent = Array.from(allMap.values()).filter(item => item.content && item.content.length > 50);
console.log(`\n有效内容项 (content>50): ${validContent.length}/${allMap.size}`);

const outPath = path.join(dir, 'all-final-content.json');
fs.writeFileSync(outPath, JSON.stringify(validContent, null, 2), 'utf-8');
console.log(`已保存: ${outPath}`);

// Output items that have content but content is just a file path
const filePathOnly = Array.from(allMap.values()).filter(item => {
  if (!item.content) return true;
  const firstLine = item.content.split('\n')[0].trim();
  return firstLine.includes('文章已写入') || firstLine.includes('written') || firstLine.includes('已存在');
});
if (filePathOnly.length > 0) {
  console.log(`\n⚠️ 可能未正确提取内容的项 (${filePathOnly.length} 项):`);
  filePathOnly.forEach(item => {
    console.log(`  ${item.itemId.padEnd(35)} ${(item.content || '').substring(0, 100).replace(/\n/g, ' ')}`);
  });
}
