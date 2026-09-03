/**
 * scan-learn-deep.js
 *
 * 扫描 data/learn-deep/ 下所有 .md 文件，输出完整路径映射。
 * 处理子目录（linux/db/vdb/）和根目录的混合情况。
 *
 * 用法: node scripts/scan-learn-deep.js
 * 输出: stdout 打印 JSON，包含文件路径、ID、堆栈信息
 */

const fs = require('fs');
const path = require('path');

const learnDeepDir = path.resolve(__dirname, '..', 'data', 'learn-deep');
const result = [];

function scanDir(dirPath, baseDir) {
  const relPath = path.relative(baseDir, dirPath);
  const entries = fs.readdirSync(dirPath, { withFileTypes: true });

  for (const entry of entries) {
    const fullPath = path.join(dirPath, entry.name);
    if (entry.isDirectory()) {
      scanDir(fullPath, baseDir);
    } else if (entry.name.endsWith('.md')) {
      const itemId = path.basename(entry.name, '.md');
      const relativePath = path.relative(baseDir, fullPath);
      const dirName = path.dirname(relativePath);
      result.push({
        itemId,
        fileName: entry.name,
        relativePath: relativePath.replace(/\\/g, '/'),
        directory: dirName === '.' ? 'root' : dirName,
        absolutePath: fullPath
      });
    }
  }
}

scanDir(learnDeepDir, learnDeepDir);

// 检查重复 ID
const idCount = {};
result.forEach(f => {
  idCount[f.itemId] = (idCount[f.itemId] || 0) + 1;
});
const duplicates = Object.entries(idCount).filter(([, c]) => c > 1).map(([id]) => id);

console.log(JSON.stringify({
  total: result.length,
  uniqueIds: Object.keys(idCount).length,
  duplicates: duplicates.length > 0 ? duplicates : null,
  files: result
}, null, 2));
