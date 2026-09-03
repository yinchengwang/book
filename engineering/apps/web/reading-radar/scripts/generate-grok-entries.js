/**
 * generate-grok-entries.js
 *
 * 从已下载的 xiaolin GitHub MD 文件生成 items-registry.js 的 grok 栈条目
 * 输出 JSON 格式，可直接嵌入到 items-registry.js
 *
 * 用法: node scripts/generate-grok-entries.js
 */

const fs = require('fs');
const path = require('path');

const RAW_DIR = path.resolve(__dirname, 'data', 'xiaolin-raw', 'github');

// Read downloaded metadata - need to re-read network items since they were overwritten
const metadata = JSON.parse(fs.readFileSync(path.join(RAW_DIR, 'downloaded-metadata.json'), 'utf-8'));

// Also scan actual files to get network items
const networkDir = path.join(RAW_DIR, 'network');
const osDir = path.join(RAW_DIR, 'os');

function scanFiles(dirPath, topicPrefix) {
  const results = [];
  function scan(currentDir, relPath) {
    const entries = fs.readdirSync(currentDir, { withFileTypes: true });
    for (const entry of entries) {
      const fullPath = path.join(currentDir, entry.name);
      const currentRel = relPath ? relPath + '/' + entry.name : entry.name;
      if (entry.isDirectory()) {
        scan(fullPath, currentRel);
      } else if (entry.name.endsWith('.md')) {
        const content = fs.readFileSync(fullPath, 'utf-8');
        const titleMatch = content.match(/^#\s+(.+)$/m);
        results.push({
          remotePath: topicPrefix + '/' + currentRel,
          itemId: currentRel.replace(/\.md$/, ''),
          title: titleMatch ? titleMatch[1].trim() : currentRel.replace(/\.md$/, ''),
        });
      }
    }
  }
  scan(dirPath, '');
  return results;
}

const networkFiles = fs.existsSync(networkDir) ? scanFiles(networkDir, 'network') : [];
const osFiles = fs.existsSync(osDir) ? scanFiles(osDir, 'os') : [];
const allFiles = [...networkFiles, ...osFiles];
console.log(`找到 ${networkFiles.length} 网络文件 + ${osFiles.length} 操作系统文件 = ${allFiles.length} 总计`);

// Category definitions
const CATEGORY_MAP = {
  // Network
  'network/1_base/': { category: '网络基础', quadrant: 'systems', ring: 'basic' },
  'network/2_http/': { category: 'HTTP/HTTPS', quadrant: 'systems', ring: 'intermediate' },
  'network/3_tcp/': { category: 'TCP', quadrant: 'systems', ring: 'intermediate' },
  'network/4_ip/': { category: 'IP', quadrant: 'systems', ring: 'basic' },
  'network/5_learn/': { category: '学习方法', quadrant: 'engineering', ring: 'basic' },
  // OS
  'os/1_hardware/': { category: '硬件基础', quadrant: 'systems', ring: 'basic' },
  'os/2_os_structure/': { category: '系统结构', quadrant: 'systems', ring: 'basic' },
  'os/3_memory/': { category: '内存管理', quadrant: 'systems', ring: 'intermediate' },
  'os/4_process/': { category: '进程线程', quadrant: 'systems', ring: 'intermediate' },
  'os/5_schedule/': { category: '调度', quadrant: 'systems', ring: 'intermediate' },
  'os/6_file_system/': { category: '文件系统', quadrant: 'systems', ring: 'intermediate' },
  'os/7_device/': { category: '设备驱动', quadrant: 'systems', ring: 'intermediate' },
  'os/8_network_system/': { category: '网络系统', quadrant: 'systems', ring: 'intermediate' },
  'os/9_linux_cmd/': { category: 'Linux 命令', quadrant: 'engineering', ring: 'basic' },
  'os/10_learn/': { category: '学习方法', quadrant: 'engineering', ring: 'basic' },
};

function getItemKey(remotePath) {
  // Remove leading topic dir (network/ or os/)
  const parts = remotePath.split('/');
  parts.shift(); // remove 'network' or 'os'
  return parts.join('/');
}

function getTags(remotePath) {
  const cat = CATEGORY_MAP[remotePath];
  if (!cat) return ['网络', '图解'];
  return [cat.category];
}

// Generate entries
const items = allFiles;
const entries = [];

for (const item of items) {
  // Skip README and non-technical pages
  if (item.itemId === 'README' || item.itemId === 'learn_network' || item.itemId === 'learn_os' ||
      item.itemId === 'draw' || item.itemId === 'what_happen_url' ||
      item.itemId === 'ping_lo' || item.itemId === 'pv_uv') {
    continue;
  }

  const key = getItemKey(item.remotePath);
  const cleanKey = key.replace(/\.md$/, '').replace(/\//g, '-');
  const simpleKey = key.replace(/[^/]+\//g, '').replace(/\.md$/, ''); // just filename without dir
  const cat = CATEGORY_MAP[item.remotePath] || { category: '其他', quadrant: 'systems', ring: 'basic' };

  const entry = {
    key: 'grok-' + simpleKey,
    product: cat.category === '网络基础' || cat.category === 'HTTP/HTTPS' || cat.category === 'TCP' || cat.category === 'IP' ? 'network' : 'os',
    stack: 'grok',
    quadrant: cat.quadrant,
    ring: cat.ring,
    title: item.title,
    desc: item.title + '。' + (cat.category === '网络基础' ? '网络基础知识。' :
                                   cat.category === 'TCP' ? 'TCP 协议核心机制。' :
                                   cat.category === '内存管理' ? '操作系统内存管理。' :
                                   cat.category === '进程线程' ? '进程与线程管理。' :
                                   '技术详解。'),
    tags: getTags(item.remotePath),
  };

  entries.push(entry);
}

// Output as JavaScript code for direct embedding
console.log('// ============================================================');
console.log('// GROK 栈条目 — 自动从 CS-Base GitHub 仓库生成');
console.log('// 总计: ' + entries.length + ' 项');
console.log('// ============================================================\n');

entries.forEach(e => {
  console.log(`  "${e.key}": { product:"${e.product}", stack:"grok", quadrant:"${e.quadrant}", ring:"${e.ring}", title:"${e.title}", desc:"${e.desc}", tags:[${e.tags.map(t => `"${t}"`).join(', ')}] },`);
});

// Save as separate file
fs.writeFileSync(path.join(__dirname, 'grok-entries-generated.js'),
  '// Auto-generated grok entries - see generate-grok-entries.js\nconst GROK_ENTRIES = ' + JSON.stringify(entries, null, 2) + ';\n'
);
console.log('\n已保存到 scripts/grok-entries-generated.js');
