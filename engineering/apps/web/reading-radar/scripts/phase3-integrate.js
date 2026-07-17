/**
 * phase3-integrate.js
 *
 * Phase 3 集成脚本：
 * 1. 备份 data/learn-deep/
 * 2. 统一收集 300 篇文章内容（从 allMap + 已写文件 + 原始文件）
 * 3. 写入 data/learn-deep/<itemId>.md（扁平路径）
 * 4. 重建 data/app/learn-deep-bundle.js
 *
 * 用法: node scripts/phase3-integrate.js
 *       node scripts/phase3-integrate.js --skip-bundle  # 只写文件不重建 bundle
 */

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const LEARN_DEEP = path.join(ROOT, 'data', 'learn-deep');
const BACKUP_DIR = path.join(ROOT, 'data', 'learn-deep-backup-' + Date.now());
const DATA_DIR = path.resolve(__dirname, 'data');
const BUNDLE_PATH = path.join(ROOT, 'data', 'app', 'learn-deep-bundle.js');

// ── 配置 ──
const skipBundle = process.argv.includes('--skip-bundle');

// ── 1. 读取 items-registry ──
const itemsPath = path.join(DATA_DIR, 'items-registry.json');
const { items } = JSON.parse(fs.readFileSync(itemsPath, 'utf-8'));
console.log(`Items: ${items.length}`);

// ── 2. 读取分类（判断 hasExistingArticle） ──
const classPath = path.join(DATA_DIR, 'classification-output.json');
const classification = JSON.parse(fs.readFileSync(classPath, 'utf-8'));
const clsMap = {};
classification.items.forEach(c => { clsMap[c.itemId] = c; });

// ── 3. 收集所有已有 .md 文件 ──
function scanMdFiles(dirPath, baseDir) {
  const results = {};
  try {
    const entries = fs.readdirSync(dirPath, { withFileTypes: true });
    for (const entry of entries) {
      const fullPath = path.join(dirPath, entry.name);
      if (entry.isDirectory()) {
        const sub = scanMdFiles(fullPath, baseDir);
        Object.assign(results, sub);
      } else if (entry.name.endsWith('.md')) {
        const itemId = path.basename(entry.name, '.md');
        if (!results[itemId]) {  // don't overwrite with subdir version
          results[itemId] = { content: fs.readFileSync(fullPath, 'utf-8'), filePath: fullPath };
        }
      }
    }
  } catch (e) { /* ignore */ }
  return results;
}

console.log('扫描现有 .md 文件...');
const existingMdFiles = scanMdFiles(LEARN_DEEP, LEARN_DEEP);
console.log(`  找到 ${Object.keys(existingMdFiles).length} 个 .md 文件`);

// ── 4. 读取 allMap 中的生成内容 ──
const allMapContent = {};
try {
  const genPath = path.join(DATA_DIR, 'all-final-content.json');
  const allContent = JSON.parse(fs.readFileSync(genPath, 'utf-8'));
  allContent.forEach(item => {
    allMapContent[item.itemId] = item.content;
  });
  console.log(`  从 allMap 读取: ${allContent.length} 项`);
} catch (e) {
  console.log(`  警告: allMap 未找到`);
}

// ── 5. 读取 retry workflow 中直接写入文件的内容（也作为来源） ──
//    这些已经通过上面的 scanMdFiles 收集了

// ── 6. 合并：决定每篇文章的最终内容 ──
// 策略：
//   - 有新生成内容的 → 用新内容（如果是已有文章，保留原内容 + 分隔线 + 补充）
//   - 没有新内容的 → 保留现有 .md 文件
//   - 既没有新内容也没有 .md 的 → 标记缺失

const CONTENT = {};
let newCount = 0;
let existingKept = 0;
let mergedCount = 0;
let missingItems = [];

for (const item of items) {
  const itemId = item.id;
  const cls = clsMap[itemId];
  const hasExisting = cls && cls.hasExistingArticle;
  const newContent = allMapContent[itemId];
  const existingContent = existingMdFiles[itemId] ? existingMdFiles[itemId].content : null;

  if (newContent && newContent.length > 100) {
    // We have generated content
    if (hasExisting && existingContent) {
      // Append as supplementary chapter
      CONTENT[itemId] = existingContent + '\n\n---\n\n## 补充内容\n\n' + newContent;
      mergedCount++;
    } else {
      // New content
      CONTENT[itemId] = newContent;
      newCount++;
    }
  } else if (existingContent) {
    // No new content, keep existing
    CONTENT[itemId] = existingContent;
    existingKept++;
  } else {
    // Check if any retry agent wrote to a subdirectory file
    // (already included in existingMdFiles scan)
    missingItems.push(itemId);
  }
}

console.log(`\n内容分配:`);
console.log(`  新生成: ${newCount}`);
console.log(`  保留现有: ${existingKept}`);
console.log(`  合并补充: ${mergedCount}`);
console.log(`  缺失: ${missingItems.length}`);

if (missingItems.length > 0) {
  console.log(`\n缺失项 (${missingItems.length}):`);
  missingItems.forEach(id => console.log(`  ${id}`));
}

// ── 7. 备份现有 learn-deep 目录 ──
console.log(`\n备份: ${LEARN_DEEP} → ${BACKUP_DIR}`);

function copyRecursive(src, dest) {
  fs.mkdirSync(dest, { recursive: true });
  const entries = fs.readdirSync(src, { withFileTypes: true });
  for (const entry of entries) {
    const srcPath = path.join(src, entry.name);
    const destPath = path.join(dest, entry.name);
    if (entry.isDirectory()) {
      copyRecursive(srcPath, destPath);
    } else {
      fs.copyFileSync(srcPath, destPath);
    }
  }
}

try {
  copyRecursive(LEARN_DEEP, BACKUP_DIR);
  console.log('  备份完成');
} catch (e) {
  console.error('  备份失败:', e.message);
}

// ── 8. 写入平面 .md 文件 ──
console.log('\n写入 .md 文件...');

// Clean old subdirectory files but keep flat ones safe
// First, collect all existing files that match item IDs
const toDelete = [];
for (const itemId in existingMdFiles) {
  const fp = existingMdFiles[itemId].filePath;
  // If it's in a subdirectory (not flat in root), mark for deletion
  const relPath = path.relative(LEARN_DEEP, fp);
  if (relPath.includes(path.sep) && relPath !== itemId + '.md') {
    toDelete.push(fp);
  }
}
// Also delete items/ and any agent-created mess
const agentDirs = ['items'];
for (const d of agentDirs) {
  const dp = path.join(LEARN_DEEP, d);
  if (fs.existsSync(dp)) toDelete.push(dp);
}

// Delete agent-created subdirectory files
let deleted = 0;
for (const fp of toDelete) {
  try {
    const stat = fs.statSync(fp);
    if (stat.isDirectory()) {
      fs.rmSync(fp, { recursive: true, force: true });
    } else {
      fs.unlinkSync(fp);
    }
    deleted++;
  } catch (e) { /* ignore */ }
}
console.log(`  清理 ${deleted} 个子目录/文件`);

// Write flat files
let written = 0;
for (const item of items) {
  const itemId = item.id;
  const content = CONTENT[itemId];
  if (!content) continue;

  const filePath = path.join(LEARN_DEEP, itemId + '.md');
  try {
    fs.writeFileSync(filePath, content, 'utf-8');
    written++;
  } catch (e) {
    console.error(`  写入失败 ${itemId}: ${e.message}`);
  }
}
console.log(`  写入 ${written} 个 .md 文件`);

// Clean up: delete root-level ds-*.md files that are duplicates of what we wrote as flat
// We already deleted subdirectory files, now we need to remove root-level ds-*.md that agents wrote
// But these ARE flat files... Let me check if we wrote array.md etc. that already exist
// Actually, those ds-*.md at root level ARE the flat format, so we should keep them or overwrite
// Our writeSync above would have overwritten them already

// ── 9. 重建 bundle ──
if (!skipBundle) {
  console.log('\n重建 learn-deep-bundle.js...');

  // Build the bundle content - use JSON.stringify for safe escaping
  const bundleMap = {};
  for (const item of items) {
    const content = CONTENT[item.id];
    if (!content) continue;
    bundleMap[item.id] = content;
  }

  const jsonStr = JSON.stringify(bundleMap, null, 2);
  const bundleContent = '"use strict";\nwindow.LEARN_DEEP_CONTENT = ' + jsonStr + ';\n';

  fs.writeFileSync(BUNDLE_PATH, bundleContent, 'utf-8');
  console.log(`  Bundle 已写入: ${BUNDLE_PATH}`);
  const size = fs.statSync(BUNDLE_PATH).size;
  console.log(`  Bundle 大小: ${(size / 1024).toFixed(0)} KB`);
  console.log(`  Bundle 条目: ${Object.keys(bundleMap).length}`);
} else {
  console.log('\n跳过 bundle 重建');
}

// ── 10. 最终验证 ──
console.log('\n=== 最终验证 ===');

// Count files in learn-deep flat dir
const finalFiles = fs.readdirSync(LEARN_DEEP).filter(f => f.endsWith('.md'));
// Remove directories from the listing
const finalDirs = fs.readdirSync(LEARN_DEEP).filter(f => {
  try { return fs.statSync(path.join(LEARN_DEEP, f)).isDirectory(); } catch(e) { return false; }
});

const finalMdCount = finalFiles.length;
const missingAfter = items.filter(item => {
  const fp = path.join(LEARN_DEEP, item.id + '.md');
  return !fs.existsSync(fp);
});

console.log(`  平面 .md 文件数: ${finalMdCount}`);
if (finalDirs.length > 0) console.log(`  遗留子目录: ${finalDirs.join(', ')}`);
console.log(`  仍有缺失: ${missingAfter.length}`);
if (missingAfter.length > 0) {
  missingAfter.forEach(item => console.log(`    ${item.id}`));
}

console.log('\nPhase 3 完成！');
if (missingAfter.length > 0) {
  console.log(`⚠️  ${missingAfter.length} 项缺失，需要手动处理。`);
} else {
  console.log('✅ 全部 300 项已覆盖！');
}
