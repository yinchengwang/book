#!/usr/bin/env node
/**
 * 摘抄数据迁移脚本 — .txt → .md
 *
 * 将 learning/notes/excerpt/ 目录下的所有 .txt 文件批量转换为带 YAML frontmatter 的 .md 文件。
 *
 * 用法:
 *   node scripts/migrate-excerpts.js [learning/notes/excerpt 路径]
 *
 * 功能:
 *   1. 扫描所有 .txt 文件
 *   2. 从文件名/目录推断书名、年份、标签
 *   3. 按段落拆分为多条摘抄
 *   4. 生成 YAML frontmatter
 *   5. 备份原始 .txt 文件为 .txt.bak
 *   6. 输出 .md 文件
 */

"use strict";

const fs = require("fs");
const path = require("path");

// ============================================================
// 配置
// ============================================================
const DEFAULT_EXCERPT_DIR = path.join(__dirname, "..", "..", "..", "learning", "notes", "excerpt");
const BACKUP_SUFFIX = ".bak";

// ============================================================
// 辅助函数
// ============================================================

/**
 * 递归扫描目录，返回所有 .txt 文件路径
 */
function scanTxtFiles(dir) {
  const results = [];
  if (!fs.existsSync(dir)) {
    console.warn(`目录不存在: ${dir}`);
    return results;
  }
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  for (const entry of entries) {
    const fullPath = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      results.push(...scanTxtFiles(fullPath));
    } else if (entry.isFile() && entry.name.endsWith(".txt") && !entry.name.endsWith(".txt.bak")) {
      results.push(fullPath);
    }
  }
  return results;
}

/**
 * 检测文件编码（简单启发式：尝试 GBK 和 UTF-8）
 */
function detectEncoding(filePath) {
  try {
    const raw = fs.readFileSync(filePath);
    // 尝试 UTF-8 解码
    const utf8Str = raw.toString("utf8");
    // 如果 UTF-8 解码后没有替换字符（�），认为是 UTF-8
    if (!utf8Str.includes("�")) {
      return { encoding: "utf8", data: raw, text: utf8Str };
    }
    // 尝试 GBK 解码
    const gbkStr = raw.toString("latin1");
    return { encoding: "gbk", data: raw, text: gbkStr };
  } catch (e) {
    console.error(`编码检测失败: ${filePath} — ${e.message}`);
    return { encoding: "utf8", text: "" };
  }
}

/**
 * 从文件名推断书名
 * 例: "代码整洁之道_程序员的职业素养.txt" → "代码整洁之道"
 *     "网络编程.txt" → "网络编程"
 */
function inferBookName(filePath) {
  const baseName = path.basename(filePath, ".txt");
  // 去掉 "_xxx" 后缀（如 _程序员的职业素养）
  const parts = baseName.split("_");
  if (parts.length > 1) {
    // 取第一部分作为书名（去掉作者/副标题）
    return parts[0].trim();
  }
  return baseName.trim();
}

/**
 * 从文件路径推断年份
 */
function inferYear(filePath, excerptDir) {
  const relPath = path.relative(excerptDir, filePath);
  const segments = relPath.split(path.sep);
  // 检查第一段是否为年份目录（如 2025/）
  if (segments.length > 0 && /^\d{4}$/.test(segments[0])) {
    return segments[0];
  }
  // 检查 reading_record 下的子目录
  const rrIdx = segments.findIndex(s => s === "reading_record");
  if (rrIdx >= 0 && rrIdx + 1 < segments.length) {
    // 如 reading_record/clean_code/xxx.txt → 年份未知
    return null;
  }
  return null;
}

/**
 * 从文件名/目录名推断标签
 */
function inferTags(filePath, bookName) {
  const tags = [];
  const relPath = path.basename(filePath, ".txt");

  // 从文件名中提取可能的标签关键词
  const keywordMap = {
    "专业主义": ["专业", "职业", "素养"],
    "责任": ["责任", "担当"],
    "数据库": ["数据库", "DB"],
    "数据结构": ["数据结构", "算法"],
    "C++": ["C++", "cpp"],
    "网络编程": ["网络", "socket", "编程"],
  };

  for (const [tag, keywords] of Object.entries(keywordMap)) {
    for (const kw of keywords) {
      if (relPath.toLowerCase().includes(kw.toLowerCase()) || bookName.includes(kw)) {
        if (!tags.includes(tag)) tags.push(tag);
        break;
      }
    }
  }

  return tags;
}

/**
 * 按段落拆分内容（空行分隔）
 */
function splitParagraphs(text) {
  // 按连续空行拆分
  const blocks = text.split(/\n\s*\n/).map(b => b.trim()).filter(b => b.length > 0);
  return blocks;
}

/**
 * 生成 YAML frontmatter
 */
function generateFrontmatter(book, author, date, tags) {
  const lines = ["---"];
  lines.push(`book: "${book}"`);
  if (author) lines.push(`author: "${author}"`);
  if (date) lines.push(`date: "${date}"`);
  if (tags && tags.length > 0) {
    lines.push(`tags: [${tags.join(", ")}]`);
  }
  lines.push("---");
  return lines.join("\n");
}

/**
 * 备份原始文件
 */
function backupFile(filePath) {
  const bakPath = filePath + BACKUP_SUFFIX;
  fs.copyFileSync(filePath, bakPath);
  return bakPath;
}

// ============================================================
// 主迁移函数
// ============================================================
function migrateFile(txtPath, excerptDir) {
  const bookName = inferBookName(txtPath);
  const year = inferYear(txtPath, excerptDir);
  const tags = inferTags(txtPath, bookName);
  const encodingInfo = detectEncoding(txtPath);
  const text = encodingInfo.text;

  if (!text || text.trim().length === 0) {
    console.log(`  ⏭ 跳过空文件: ${path.relative(excerptDir, txtPath)}`);
    return { skipped: 1 };
  }

  // 备份原始文件
  const bakPath = backupFile(txtPath);
  console.log(`  📦 已备份: ${path.basename(txtPath)} → ${path.basename(bakPath)}`);

  // 拆分段落
  const paragraphs = splitParagraphs(text);

  // 生成 frontmatter
  const frontmatter = generateFrontmatter(bookName, null, year, tags);

  // 生成 .md 内容
  const mdParts = paragraphs.map((p, i) => {
    if (paragraphs.length === 1) return p;
    return `## 摘抄 ${i + 1}\n\n${p}`;
  });
  const mdContent = frontmatter + "\n\n" + mdParts.join("\n\n");

  // 写入 .md 文件
  const mdPath = txtPath.replace(/\.txt$/, ".md");
  fs.writeFileSync(mdPath, mdContent, "utf8");
  console.log(`  ✅ 已生成: ${path.relative(excerptDir, mdPath)} (${paragraphs.length} 条摘抄)`);

  return { migrated: 1, excerpts: paragraphs.length };
}

// ============================================================
// 入口
// ============================================================
function main() {
  const excerptDir = process.argv[2] || DEFAULT_EXCERPT_DIR;
  const absExcerptDir = path.resolve(excerptDir);

  console.log(`📝 摘抄迁移脚本`);
  console.log(`   数据目录: ${absExcerptDir}`);
  console.log("");

  const txtFiles = scanTxtFiles(absExcerptDir);

  if (txtFiles.length === 0) {
    console.log("未发现 .txt 文件，无需迁移。");
    return;
  }

  console.log(`发现 ${txtFiles.length} 个 .txt 文件\n`);

  let totalMigrated = 0;
  let totalExcerpts = 0;
  let totalSkipped = 0;

  for (const file of txtFiles) {
    const result = migrateFile(file, absExcerptDir);
    totalMigrated += result.migrated || 0;
    totalExcerpts += result.excerpts || 0;
    totalSkipped += result.skipped || 0;
  }

  console.log(`\n🎉 迁移完成`);
  console.log(`   转换文件: ${totalMigrated}`);
  console.log(`   生成摘抄: ${totalExcerpts} 条`);
  console.log(`   跳过空文件: ${totalSkipped}`);
}

main();
