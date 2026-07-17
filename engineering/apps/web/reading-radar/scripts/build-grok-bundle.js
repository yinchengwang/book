/**
 * build-grok-bundle.js
 *
 * 将 data/learn-deep/grok/ 下的所有 .md 文件构建到 learn-deep-bundle.js 中
 * 与 phase3-integrate.js 互补：phase3-integrate 处理已有的 300 项，
 * 此脚本专门处理 grok 栈的 66 篇新文章
 *
 * 用法: node scripts/build-grok-bundle.js
 */

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const GROK_DIR = path.join(ROOT, 'data', 'learn-deep', 'grok');
const BUNDLE_PATH = path.join(ROOT, 'data', 'app', 'learn-deep-bundle.js');

// ── 0. 读取 items-registry.js 获取所有 grok id ──
const REGISTRY_PATH = path.join(ROOT, 'data', 'app', 'items-registry.js');
let grokRegistryIds = [];
try {
  const regRaw = fs.readFileSync(REGISTRY_PATH, 'utf-8');
  // 匹配 "grok-xxx": { 形式的 id
  const idMatches = regRaw.match(/"(grok-[^"]+)":\s*{/g) || [];
  grokRegistryIds = idMatches.map(m => m.replace('"', '').replace('": {', ''));
  console.log(`  从 registry 读取到 ${grokRegistryIds.length} 个 grok id`);
} catch (e) {
  console.log(`  警告: 无法读取 registry: ${e.message}，将使用文件名作为 key`);
}

// ── 1. 读取现有 bundle ──
console.log('读取现有 bundle...');
let existingBundle = {};
try {
  const existingRaw = fs.readFileSync(BUNDLE_PATH, 'utf-8');
  // Extract the LEARN_DEEP_CONTENT object
  const match = existingRaw.match(/window\.LEARN_DEEP_CONTENT\s*=\s*({[\s\S]*});\s*$/);
  if (match) {
    existingBundle = JSON.parse(match[1]);
    console.log(`  现有条目: ${Object.keys(existingBundle).length}`);
  }
} catch (e) {
  console.log(`  警告: 无法读取现有 bundle: ${e.message}`);
}

// ── 2. 扫描 grok/ 下的所有 .md 文件 ──
// 建立 文件名 → registry id 的映射
function buildFileNameMap(registryIds) {
  const map = {}; // itemId (文件名) → registry id
  registryIds.forEach(id => {
    const name = id.replace('grok-', ''); // 去掉 grok- 前缀
    // name 可能是 "base_how_select" 或 "how_os_deal_network_package"
    // 文件名可能是 "how_select.md" 或 "how_os_deal_network_package.md"
    // 策略：尝试匹配最后一段
    const parts = name.split('_');
    const fileName = parts[parts.length - 1]; // 取最后一段，如 "how_select"
    if (!map[fileName]) map[fileName] = id;
  });
  return map;
}

const fileNameToId = buildFileNameMap(grokRegistryIds);

function scanGroks(dirPath) {
  const results = {};
  function scan(currentDir, relPath) {
    try {
      const entries = fs.readdirSync(currentDir, { withFileTypes: true });
      for (const entry of entries) {
        const fullPath = path.join(currentDir, entry.name);
        const currentRel = relPath ? relPath + '/' + entry.name : entry.name;
        if (entry.isDirectory()) {
          scan(fullPath, currentRel);
        } else if (entry.name.endsWith('.md')) {
          const content = fs.readFileSync(fullPath, 'utf-8');
          const itemId = path.basename(entry.name, '.md');
          // 优先使用 registry id，找不到则回退到文件名
          let bundleKey = fileNameToId[itemId] || 'grok-' + itemId;
          results[bundleKey] = content;
        }
      }
    } catch (e) { /* ignore */ }
  }
  scan(dirPath, '');
  return results;
}

console.log('\n扫描 grok/ 目录...');
const grokContent = scanGroks(GROK_DIR);
console.log(`  找到 ${Object.keys(grokContent).length} 篇 grok 文章`);

// ── 3. 合并 ──
const merged = { ...existingBundle, ...grokContent };
console.log(`\n合并后总条目: ${Object.keys(merged).length}`);
console.log(`  原有: ${Object.keys(existingBundle).length}`);
console.log(`  新增 grok: ${Object.keys(grokContent).length}`);

// ── 4. 写入 bundle ──
const jsonStr = JSON.stringify(merged, null, 2);
const bundleContent = '"use strict";\nwindow.LEARN_DEEP_CONTENT = ' + jsonStr + ';\n';
fs.writeFileSync(BUNDLE_PATH, bundleContent, 'utf-8');
console.log(`\nBundle 已写入: ${BUNDLE_PATH}`);
const size = fs.statSync(BUNDLE_PATH).size;
console.log(`Bundle 大小: ${(size / 1024 / 1024).toFixed(2)} MB (${size.toLocaleString()} bytes)`);

// ── 5. 列出新增的 grok 条目 ──
const grokKeys = Object.keys(merged).filter(k => k.startsWith('grok-'));
console.log(`\ngrok 条目列表 (${grokKeys.length}):`);
const grouped = {};
grokKeys.forEach(k => {
  const name = k.replace('grok-', '');
  // Group by topic
  let topic = '其他';
  if (name.includes('tcp_')) topic = 'TCP';
  else if (name.includes('http')) topic = 'HTTP';
  else if (name.includes('ip_')) topic = 'IP';
  else if (name.includes('quic') || name.includes('challenge') || name.includes('syn_') ||
           name.includes('time_wait') || name.includes('port') || name.includes('drop') ||
           name.includes('down_and_crash') || name.includes('three_fin') || name.includes('unplug') ||
           name.includes('recv_syn') || name.includes('isn') || name.includes('out_of_order'))
    topic = 'TCP';
  else if (name.includes('how_os_deal') || name.includes('tcp_ip_model')) topic = '网络基础';
  else if (name.includes('cpu_') || name.includes('storage') || name.includes('float') ||
           name.includes('soft_interrupt') || name.includes('how_cpu_') || name.includes('make_cpu'))
    topic = '硬件基础';
  else if (name.includes('vmem') || name.includes('malloc') || name.includes('mem_reclaim') ||
           name.includes('cache_lru') || name.includes('alloc_mem'))
    topic = '内存管理';
  else if (name.includes('process') || name.includes('thread') || name.includes('deadlock') ||
           name.includes('multithread') || name.includes('pessim') || name.includes('create_thread'))
    topic = '进程线程';
  else if (name.includes('schedule')) topic = '调度';
  else if (name.includes('file_system') || name.includes('pagecache')) topic = '文件系统';
  else if (name.includes('device')) topic = '设备驱动';
  else if (name.includes('zero_copy') || name.includes('reactor') || name.includes('poll_epoll') ||
           name.includes('hash'))
    topic = '网络系统';
  else if (name.includes('linux_network') || name.includes('pv_uv')) topic = 'Linux 命令';
  else if (name.includes('linux_vs')) topic = '系统结构';

  if (!grouped[topic]) grouped[topic] = [];
  grouped[topic].push(k);
});

for (const [topic, keys] of Object.entries(grouped)) {
  console.log(`  ${topic}: ${keys.length} 篇`);
  keys.slice(0, 3).forEach(k => console.log(`    - ${k.replace('grok-', '')}`));
  if (keys.length > 3) console.log(`    ... 还有 ${keys.length - 3} 篇`);
}

console.log('\n=== Grok Bundle 构建完成 ===');
