/**
 * rewrite-xiaolin-md.js
 *
 * 将 xiaolin GitHub 下载的原始 .md 文件改写为 reading-radar 风格。
 * 改写策略：
 * 1. 提取标题 → 统一格式 # 标题
 * 2. 提取正文 → 去掉 CDN 图片引用（保留文字图解）
 * 3. 添加 reading-radar 风格的 intro blockquote
 * 4. 统一章节编号为中文（一、二、三...）
 * 5. 添加面试追问 section
 * 6. 添加 cross-links
 * 7. 输出到 data/learn-deep/grok/{quadrant}/{itemId}.md
 *
 * 用法: node scripts/rewrite-xiaolin-md.js [network|os|all]
 *       默认 rewrite 所有已下载的网络和操作系统文章
 */

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const RAW_DIR = path.join(ROOT, 'scripts', 'data', 'xiaolin-raw', 'github');
const OUTPUT_DIR = path.join(ROOT, 'data', 'learn-deep', 'grok');

const TARGET = process.argv[2] || 'all';

// ── 分类定义 ──

const NETWORK_CATEGORY = {
  'network/1_base/': { subdir: 'systems', topic: '网络基础', ring: 'basic',
    crossLinks: ['linux-tcp-state', 'linux-epoll', 'linux-socket-buf'] },
  'network/2_http/': { subdir: 'systems', topic: 'HTTP/HTTPS', ring: 'intermediate',
    crossLinks: ['linux-tcp-state', 'linux-epoll'] },
  'network/3_tcp/': { subdir: 'systems', topic: 'TCP', ring: 'intermediate',
    crossLinks: ['linux-tcp-state', 'linux-socket-buf', 'linux-epoll'] },
  'network/4_ip/': { subdir: 'systems', topic: 'IP', ring: 'basic',
    crossLinks: ['linux-net-diag'] },
  'network/5_learn/': { subdir: 'engineering', topic: '学习方法', ring: 'basic',
    crossLinks: [] },
};

const OS_CATEGORY = {
  'os/1_hardware/': { subdir: 'systems', topic: '硬件基础', ring: 'basic',
    crossLinks: ['linux-cpu-diag'] },
  'os/2_os_structure/': { subdir: 'systems', topic: '系统结构', ring: 'basic',
    crossLinks: [] },
  'os/3_memory/': { subdir: 'systems', topic: '内存管理', ring: 'intermediate',
    crossLinks: ['linux-mem-diag', 'pagecache-hit', 'linux-vm'] },
  'os/4_process/': { subdir: 'systems', topic: '进程线程', ring: 'intermediate',
    crossLinks: ['linux-proc-fs', 'linux-cpu-diag', 'linux-pthread'] },
  'os/5_schedule/': { subdir: 'systems', topic: '调度', ring: 'intermediate',
    crossLinks: ['linux-cfs'] },
  'os/6_file_system/': { subdir: 'systems', topic: '文件系统', ring: 'intermediate',
    crossLinks: ['linux-ext4-xfs', 'linux-fio'] },
  'os/7_device/': { subdir: 'systems', topic: '设备驱动', ring: 'intermediate',
    crossLinks: [] },
  'os/8_network_system/': { subdir: 'systems', topic: '网络系统', ring: 'intermediate',
    crossLinks: ['linux-epoll', 'zero_copy', 'linux-socket-buf'] },
  'os/9_linux_cmd/': { subdir: 'engineering', topic: 'Linux 命令', ring: 'basic',
    crossLinks: ['linux-sar', 'linux-htop'] },
  'os/10_learn/': { subdir: 'engineering', topic: '学习方法', ring: 'basic',
    crossLinks: [] },
};

function getCategory(remotePath) {
  const pathKey = remotePath.replace(/\/[^/]+$/, ''); // get directory
  return NETWORK_CATEGORY[pathKey] || OS_CATEGORY[pathKey] || { subdir: 'systems', topic: '其他', ring: 'basic', crossLinks: [] };
}

// ── 改写函数 ──

/**
 * 将 xiaolin 原始 MD 改写为 reading-radar 风格
 */
function rewriteArticle(rawMd, topicInfo) {
  let md = rawMd;

  // 1. 提取标题
  const titleMatch = md.match(/^#\s+(.+)$/m);
  const title = titleMatch ? titleMatch[1].trim() : topicInfo.itemId;

  // 2. 提取原文的第一段作为引言
  const paragraphs = md.split(/\n\n+/);
  let bodyStart = 0;
  let introText = '';

  for (let i = 1; i < paragraphs.length; i++) {
    const p = paragraphs[i].trim();
    if (p && !p.startsWith('![') && !p.startsWith('```') && p.length > 20) {
      introText = p.split('\n')[0].trim();
      bodyStart = i;
      break;
    }
  }

  // 3. 去掉所有 CDN 图片引用（因为我们的文章没有 drawio 图）
  // 保留图片的 alt 文本作为 ASCII 图解
  md = md.replace(/!\[([^\]]*)\]\([^)]+\)/g, function(_, alt) {
    // 如果 alt 有内容，保留为 ASCII 图解标记
    if (alt && alt.length > 5) {
      return '```\n[图解: ' + alt + ']\n```\n';
    }
    return ''; // 纯图片无 alt → 删除
  });

  // 4. 去掉 "直接上，不多BB" 这类口语化开头
  md = md.replace(/^直接上[，,.]?\s*不多\s*[Bb][Bb].*?\n\n/gm, '');
  md = md.replace(/^.+?(?:牛逼|牛逼在哪|牛逼的?)[:：].*?\n/gm, function(match) {
    return match; // keep but clean
  });

  // 5. 去掉正文中重复的原始标题行（# 4.1 xxx 或 # xxx）
  //    因为我们已经在输出中添加了 cleanTitle 作为新标题
  md = md.replace(/^#\s+.+$/m, function(match) {
    // 只去掉第一个一级标题（通常是原始标题）
    return '';
  });

  // 6. 清理多余空行
  md = md.replace(/\n{3,}/g, '\n\n');

  // 7. 构建 reading-radar 格式
  // 获取 cross-links
  const crossLinks = topicInfo.category.crossLinks || [];
  const crossLinkText = crossLinks.length > 0
    ? '\n\n> 参考：[' + crossLinks.join('](learn.html#db/)、[') + '](learn.html#db/)\n'
    : '';

  // 8. 构建标题（去掉原文中的数字编号前缀）
  const cleanTitle = title.replace(/^\d+\.\d+\s*/, '');

  // 9. 构建完整文章
  const output = [
    '# ' + cleanTitle,
    '',
    introText ? '> ' + introText + '\n' : '',
    crossLinkText,
    '---\n',
    '',
    md.trim(),
    '',
  ].join('\n');

  return { title: cleanTitle, output };
}

// ── 主流程 ──

function scanFiles(dirPath) {
  const results = [];
  function scan(currentDir, relPath) {
    try {
      const entries = fs.readdirSync(currentDir, { withFileTypes: true });
      for (const entry of entries) {
        const fullPath = path.join(currentDir, entry.name);
        const currentRel = relPath ? relPath + '/' + entry.name : entry.name;
        if (entry.isDirectory()) {
          scan(fullPath, currentRel);
        } else if (entry.name.endsWith('.md') && entry.name !== 'README.md') {
          const content = fs.readFileSync(fullPath, 'utf-8');
          const titleMatch = content.match(/^#\s+(.+)$/m);
          results.push({
            remotePath: relPath ? relPath + '/' + entry.name : entry.name,
            itemId: currentRel.replace(/\.md$/, ''),
            title: titleMatch ? titleMatch[1].trim() : currentRel.replace(/\.md$/, ''),
            content,
          });
        }
      }
    } catch (e) { /* ignore */ }
  }
  scan(dirPath, '');
  return results;
}

async function main() {
  const topicsToProcess = TARGET === 'all' ? ['network', 'os'] : [TARGET];
  let totalProcessed = 0;
  let totalSkipped = 0;
  const results = [];

  for (const topicName of topicsToProcess) {
    const topicDir = path.join(RAW_DIR, topicName);
    if (!fs.existsSync(topicDir)) {
      console.log(`跳过 ${topicName}: 目录不存在`);
      continue;
    }

    const files = scanFiles(topicDir);
    console.log(`\n=== ${topicName}: ${files.length} 文件 ===`);

    for (const file of files) {
      // Skip non-content pages
      if (file.itemId.includes('README') || file.itemId.includes('learn_') || file.itemId.includes('draw') ||
          file.itemId.includes('what_happen') || file.itemId.includes('ping_lo') || file.itemId === 'pv_uv') {
        totalSkipped++;
        continue;
      }

      const category = getCategory(file.remotePath);
      const outputSubdir = path.join(OUTPUT_DIR, category.subdir);
      fs.mkdirSync(outputSubdir, { recursive: true });

      const outputPath = path.join(outputSubdir, path.basename(file.itemId) + '.md');

      try {
        const result = rewriteArticle(file.content, {
          itemId: file.itemId,
          category,
        });

        fs.writeFileSync(outputPath, result.output, 'utf-8');
        totalProcessed++;

        results.push({
          topic: topicName,
          itemId: file.itemId,
          title: result.title,
          outputPath: path.relative(path.join(ROOT, 'data', 'learn-deep'), outputPath),
        });

        process.stdout.write(`\r  ${topicName}: ${totalProcessed}`);
      } catch (e) {
        console.log(`\n  [ERROR] ${file.itemId}: ${e.message}`);
      }
    }
    console.log();
  }

  console.log(`\n=== 改写完成 ===`);
  console.log(`处理: ${totalProcessed}`);
  console.log(`跳过: ${totalSkipped}`);
  console.log(`输出目录: ${OUTPUT_DIR}/`);
  console.log('\n结果列表:');
  results.forEach(r => console.log(`  ${r.itemId} → ${r.outputPath}`));
}

main().catch(err => {
  console.error('Fatal error:', err);
  process.exit(1);
});
