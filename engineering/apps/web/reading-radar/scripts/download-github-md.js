/**
 * download-github-md.js
 *
 * 从 xiaolincoder/CS-Base GitHub 仓库下载网络/操作系统图解系列
 * 的 Markdown 源文件，保存到本地供后续改写使用。
 *
 * 用法: node scripts/download-github-md.js [network|os|all]
 *       默认下载 network + os
 */

const https = require('https');
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const RAW_DIR = path.join(ROOT, 'scripts', 'data', 'xiaolin-raw', 'github');
const TARGET_STACK = process.argv[2] || 'all';

const REPO = 'xiaolincoder/CS-Base';
const BRANCH = 'main';

// Topics to download
const TOPICS = {
  network: {
    remotePath: 'network',
    stack: 'grok',
    quadrant: 'systems',
    ring: 'basic',
    label: '图解网络',
  },
  os: {
    remotePath: 'os',
    stack: 'grok',
    quadrant: 'systems',
    ring: 'basic',
    label: '图解操作系统',
  },
};

function fetchJson(url) {
  return new Promise((resolve, reject) => {
    https.get(url, {
      headers: {
        'User-Agent': 'Mozilla/5.0',
        'Accept': 'application/vnd.github.v3+json',
      },
    }, res => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => resolve(JSON.parse(data)));
    }).on('error', reject);
  });
}

async function downloadFile(repoPath, remotePath, localPath) {
  const url = `https://raw.githubusercontent.com/${REPO}/${BRANCH}/${remotePath}`;
  return new Promise((resolve, reject) => {
    https.get(url, {
      headers: { 'User-Agent': 'Mozilla/5.0' },
    }, res => {
      if (res.statusCode !== 200) {
        reject(new Error(`HTTP ${res.statusCode}: ${url}`));
        return;
      }
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        fs.writeFileSync(localPath, data, 'utf-8');
        resolve({ size: data.length, url });
      });
    }).on('error', reject);
  });
}

async function downloadTopic(topicName, topicConfig) {
  console.log(`\n=== ${topicConfig.label} (${topicName}) ===`);

  // Use GitHub contents API (recursive) to list all files in the topic directory
  const contentsUrl = `https://api.github.com/repos/${REPO}/contents/${topicConfig.remotePath}?ref=${BRANCH}`;
  let allFiles = [];
  await listDirectoryContents(contentsUrl, allFiles);

  const mdFiles = allFiles.filter(f => f.name.endsWith('.md'));
  console.log(`  找到 ${mdFiles.length} 个 .md 文件`);

  // Create local directory
  const localTopicDir = path.join(RAW_DIR, topicName);
  fs.mkdirSync(localTopicDir, { recursive: true });

  let successCount = 0;
  let failCount = 0;
  const results = [];

  for (const file of mdFiles) {
    const localFile = path.join(localTopicDir, file.path);
    const localDir = path.dirname(localFile);
    fs.mkdirSync(localDir, { recursive: true });

    try {
      const result = await downloadFile(REPO, file.path, localFile);
      successCount++;
      results.push({
        itemId: file.path.replace(/\.md$/, ''),
        remotePath: file.path,
        localPath: path.relative(RAW_DIR, localFile),
        size: result.size,
        title: '', // We'll extract title from content later
      });
    } catch (err) {
      failCount++;
      results.push({
        itemId: file.path.replace(/\.md$/, ''),
        remotePath: file.path,
        error: err.message,
      });
    }

    process.stdout.write(`\r  ${topicName}: ${successCount + failCount}/${mdFiles.length}`);
  }
  console.log();

  return results;
}

/**
 * Recursively list all files under a GitHub directory using contents API
 */
async function listDirectoryContents(url, resultArray, depth = 0) {
  if (depth > 5) return; // Safety limit

  try {
    const items = await fetchJson(url);
    for (const item of items) {
      if (item.type === 'file') {
        resultArray.push(item);
      } else if (item.type === 'dir') {
        await listDirectoryContents(item.url, resultArray, depth + 1);
      }
    }
  } catch (e) {
    // Ignore individual failures, continue with other branches
  }
}

async function main() {
  fs.mkdirSync(RAW_DIR, { recursive: true });

  const topicsToDownload = TARGET_STACK === 'all'
    ? ['network', 'os']
    : [TARGET_STACK];

  let allResults = [];
  let totalSuccess = 0;
  let totalFail = 0;

  for (const topicName of topicsToDownload) {
    const config = TOPICS[topicName];
    if (!config) {
      console.log(`跳过未知专题: ${topicName}`);
      continue;
    }

    const results = await downloadTopic(topicName, config);
    allResults.push(...results);
    totalSuccess += results.filter(r => !r.error).length;
    totalFail += results.filter(r => r.error).length;
  }

  // Extract titles from downloaded files
  console.log('\n提取标题...');
  for (const result of allResults) {
    if (result.error) continue;
    const localFile = path.join(RAW_DIR, result.localPath);
    try {
      const content = fs.readFileSync(localFile, 'utf-8');
      const titleMatch = content.match(/^#\s+(.+)$/m);
      result.title = titleMatch ? titleMatch[1].trim() : result.itemId;
    } catch (e) {
      result.title = result.itemId;
    }
  }

  // Save metadata
  const output = {
    downloadedAt: new Date().toISOString(),
    repo: REPO,
    branch: BRANCH,
    topics: topicsToDownload,
    totalFiles: allResults.length,
    successCount: totalSuccess,
    failCount: totalFail,
    items: allResults.map(r => ({
      itemId: r.itemId,
      title: r.title,
      remotePath: r.remotePath,
      localPath: r.localPath,
      size: r.size,
      error: r.error,
    })),
  };

  const outputPath = path.join(RAW_DIR, 'downloaded-metadata.json');
  fs.writeFileSync(outputPath, JSON.stringify(output, null, 2), 'utf-8');

  console.log(`\n=== 下载完成 ===`);
  console.log(`成功: ${totalSuccess}`);
  console.log(`失败: ${totalFail}`);
  console.log(`原始文件: ${RAW_DIR}/`);
  console.log(`元数据: ${outputPath}`);

  if (totalFail > 0) {
    console.log('\n失败列表:');
    allResults.filter(r => r.error).forEach(r => console.log(`  ${r.itemId}: ${r.error}`));
  }
}

main().catch(err => {
  console.error('Fatal error:', err);
  process.exit(1);
});
