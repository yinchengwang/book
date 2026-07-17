/**
 * merge-existing-articles.js
 *
 * 将分类结果与已有 .md 文件列表合并，标记每个知识点是否已有文章。
 *
 * 用法: node scripts/merge-existing-articles.js
 * 输出: 更新 scripts/data/classification-output.json 中的 hasExistingArticle 字段
 */

const fs = require('fs');
const path = require('path');

const classificationPath = path.resolve(__dirname, 'data', 'classification-output.json');
const mdFilesPath = path.resolve(__dirname, 'data', 'md-files.json');

const classification = JSON.parse(fs.readFileSync(classificationPath, 'utf-8'));
const mdData = JSON.parse(fs.readFileSync(mdFilesPath, 'utf-8'));

// Build set of item IDs that have .md files
const mdIds = new Set(mdData.files.map(f => f.itemId));

let withArticle = 0;
classification.items.forEach(item => {
  const hasArticle = mdIds.has(item.itemId);
  item.hasExistingArticle = hasArticle;
  if (hasArticle) withArticle++;
});

fs.writeFileSync(classificationPath, JSON.stringify(classification, null, 2), 'utf-8');
console.log(`分类结果已更新: ${classification.total} 个知识点`);
console.log(`  有已有文章: ${withArticle}`);
console.log(`  无文章: ${classification.total - withArticle}`);
console.log(`  Strong: ${classification.strong}`);
console.log(`  Medium: ${classification.medium}`);
console.log(`  Weak: ${classification.weak}`);
