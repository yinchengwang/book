/**
 * 图解系列分类迁移脚本 v3
 * 直接通过 Node require 加载 TS 文件（用 .ts 后缀伪装），
 * 然后用 JSON-like 数组提取
 */

const fs = require('fs');
const path = require('path');

// 读取原始文件
const filePath = path.join(__dirname, '../../src/data/learn_deep_index.ts');
let content = fs.readFileSync(filePath, 'utf-8');

// 提取 export const learnDeepList 这一行 + 数组内容
// 数组的起始: '[', 结束: 最后一个 '];' 之前的 ']'
const arrStartIdx = content.indexOf('export const learnDeepList');
if (arrStartIdx === -1) {
  console.error('找不到 learnDeepList 声明');
  process.exit(1);
}

// 从 [ 开始扫描，到最后一个 ] 结束
const bracketStart = content.indexOf('[', arrStartIdx);
let bracketEnd = content.lastIndexOf('];');
if (bracketStart === -1 || bracketEnd === -1) {
  console.error('找不到数组边界');
  process.exit(1);
}

const arrText = content.substring(bracketStart + 1, bracketEnd);

// 用 JSON5 风格的解析 - 写入临时 .json 文件，然后 require
// 但内容是 TS 格式（带转义），所以更稳妥的是手工解析

// 解析器：逐字符扫描，跟踪花括号深度和字符串边界
function parseArticles(text) {
  const articles = [];
  let i = 0;
  const n = text.length;

  while (i < n) {
    // 跳过空白
    while (i < n && /\s/.test(text[i])) i++;
    // 跳过逗号
    while (i < n && text[i] === ',') {
      i++;
      while (i < n && /\s/.test(text[i])) i++;
    }
    if (i >= n) break;

    if (text[i] !== '{') {
      i++;
      continue;
    }

    // 开始一个对象，扫描匹配的 '}'
    let depth = 1;
    let j = i + 1;
    let inString = false;
    let stringChar = '';
    let escape = false;

    while (j < n && depth > 0) {
      const c = text[j];
      if (escape) {
        escape = false;
        j++;
        continue;
      }
      if (c === '\\') {
        escape = true;
        j++;
        continue;
      }
      if (!inString && (c === '"' || c === "'")) {
        inString = true;
        stringChar = c;
        j++;
        continue;
      }
      if (inString && c === stringChar) {
        inString = false;
        j++;
        continue;
      }
      if (!inString) {
        if (c === '{') depth++;
        else if (c === '}') depth--;
      }
      j++;
    }

    if (depth !== 0) {
      console.error('未闭合的条目，从位置', i, '已找到', articles.length, '篇文章');
      break;
    }

    // 提取条目文本（不包含前后的空白）
    const entryText = text.substring(i, j);
    articles.push(entryText);
    i = j;
  }

  return articles;
}

const articles = parseArticles(arrText);
console.log(`解析到 ${articles.length} 篇文章`);

if (articles.length === 0) {
  // 调试：打印前 1000 个字符
  console.error('前 1000 字符:', arrText.substring(0, 1000));
  process.exit(1);
}

// 分类逻辑
function classifyArticle(title, slug, currentCategory) {
  const lowerTitle = title.toLowerCase();

  // 向量数据库关键词（优先匹配）
  const vdbKeywords = [
    '向量', 'hnsw', 'ivf', 'diskann', 'milvus', 'pinecone', 'faiss', 'chroma',
    'pgvector', 'weaviate', 'qdrant', '量化', 'vector', '近似最近邻',
    'lance', 'vearch', 'proxima'
  ];

  // Linux 关键词
  const linuxKeywords = [
    'linux', 'page cache', 'procfs', 'perf', 'blktrace', 'htop', 'sar',
    'io 调度', 'io scheduler', 'cfs', 'mmap', 'cgroup', 'io_uring',
    'ext4', 'xfs', 'btrfs', 'nfs', 'lvm', 'raid', '块设备', 'fio', 'fuse', 'fsync',
    'strace', 'seccomp', 'ebpf', 'hugepages', 'direct io', '直接 io',
    'oom killer', '系统调用', 'flamegraph', '火焰图', 'proc/stat', 'proc/meminfo',
    'proc/diskstats', 'io 追踪', 'cgroup v2'
  ];

  // 网络关键词
  const networkKeywords = [
    'tcp', 'socket', 'http', 'https', 'ssl', 'tls', 'epoll',
    'reactor', 'proactor', '零拷贝', 'zero copy', 'xdp', 'iptables',
    'nftables', 'netfilter', '连接池', 'backlog', 'timewait',
    '三次握手', '四次挥手', '重传', '流量控制', '拥塞控制', 'udp',
    'websocket', 'quic', 'http/2', 'http/3', 'http2', 'http3', 'rpc', 'dns', 'cdn',
    '负载均衡', 'nginx', 'veth', 'bridge', '网络命名空间', 'unix domain',
    'tcp状态机', 'socket缓冲区', '抓包', 'tcpdump', 'reuseport',
    'so_reuseport'
  ];

  // 系统关键词
  const systemKeywords = [
    'cpu缓存', 'cpu cache', '预读', 'prefetch', '分支预测',
    '中断', 'interrupt', '软中断', 'softirq', '锁', 'lock',
    'mutex', '自旋锁', 'spinlock', '读写锁', 'rcu', '内存屏障', 'memory barrier',
    '虚拟内存', '进程', '线程', '调度', 'schedule', '上下文切换', 'context switch',
    'numa', 'oom', '内存泄漏', 'malloc', 'jemalloc', 'tcmalloc', 'glibc',
    '内核', 'kernel', '死锁', '一致性', '浮点数', '0.1 + 0.2',
    'cpu是', '键盘敲入', '4gb物理内存', '进程最多',
    'cpu 调度', '调度器', '时间片', '为什么0.1'
  ];

  // 检查关键词 - 优先向量数据库
  for (const kw of vdbKeywords) {
    if (lowerTitle.includes(kw)) return 'vdb';
  }
  for (const kw of linuxKeywords) {
    if (lowerTitle.includes(kw)) return 'linux';
  }
  for (const kw of networkKeywords) {
    if (lowerTitle.includes(kw)) return 'network';
  }
  for (const kw of systemKeywords) {
    if (lowerTitle.includes(kw)) return 'system';
  }

  // 默认返回当前分类
  if (currentCategory === '综合') {
    return 'system';
  }
  if (currentCategory === '数据库') {
    return 'database';
  }

  // 算法、C、Python、C++ 保持原样
  const map = { '算法': 'algorithm', 'C': 'c', 'Python': 'python', 'C++': 'cpp' };
  return map[currentCategory] || currentCategory;
}

// 处理每篇文章
const categoryCount = {
  linux: 0, network: 0, system: 0, database: 0, vdb: 0,
  algorithm: 0, c: 0, python: 0, cpp: 0
};

const newArticles = articles.map((entryText) => {
  // 提取 slug、title、category（用更可靠的方式）
  const slugMatch = entryText.match(/"slug":\s*"((?:[^"\\]|\\.)*)"/);
  const titleMatch = entryText.match(/"title":\s*"((?:[^"\\]|\\.)*)"/);
  const categoryMatch = entryText.match(/"category":\s*"((?:[^"\\]|\\.)*)"/);

  if (!slugMatch || !titleMatch || !categoryMatch) {
    console.warn('无法解析:', entryText.substring(0, 80));
    return entryText;
  }

  const unescape = (s) => s.replace(/\\(.)/g, '$1');
  const slug = unescape(slugMatch[1]);
  const title = unescape(titleMatch[1]);
  const oldCategory = unescape(categoryMatch[1]);

  const newCategory = classifyArticle(title, slug, oldCategory);
  categoryCount[newCategory]++;

  // 替换 category 字段
  return entryText.replace(
    /"category":\s*"((?:[^"\\]|\\.)*)"/,
    `"category": "${newCategory}"`
  );
});

// 输出统计
console.log('\n分类统计：');
console.log('  Linux:', categoryCount.linux);
console.log('  Network:', categoryCount.network);
console.log('  System:', categoryCount.system);
console.log('  Database:', categoryCount.database);
console.log('  VDB:', categoryCount.vdb);
console.log('  Algorithm:', categoryCount.algorithm);
console.log('  C:', categoryCount.c);
console.log('  Python:', categoryCount.python);
console.log('  C++:', categoryCount.cpp);
const total = Object.values(categoryCount).reduce((a, b) => a + b, 0);
console.log('  Total:', total);

// 重建数组内容
const newArrText = newArticles.join(',\n  ');

// 拼接新文件
const beforeArr = content.substring(0, bracketStart + 1);
const afterArr = content.substring(bracketEnd);

// 添加分类常量定义到头部
const categoryDefs = `
// ============ 分类常量定义 ============
export const CATEGORY_ORDER = ['linux', 'network', 'system', 'database', 'vdb', 'algorithm', 'c', 'python', 'cpp'] as const

export type CategoryId = typeof CATEGORY_ORDER[number]

export interface CategoryMeta {
  id: CategoryId
  name: string        // 显示名称
  slug: string        // URL slug
  count: number       // 文章数量
}

export const CATEGORIES: CategoryMeta[] = [
  { id: 'linux', name: 'Linux', slug: 'linux', count: 0 },
  { id: 'network', name: '网络', slug: 'network', count: 0 },
  { id: 'system', name: '系统', slug: 'system', count: 0 },
  { id: 'database', name: '数据库', slug: 'database', count: 0 },
  { id: 'vdb', name: '向量数据库', slug: 'vdb', count: 0 },
  { id: 'algorithm', name: '算法', slug: 'algorithm', count: 0 },
  { id: 'c', name: 'C', slug: 'c', count: 0 },
  { id: 'python', name: 'Python', slug: 'python', count: 0 },
  { id: 'cpp', name: 'C++', slug: 'cpp', count: 0 },
]

export interface LearnDeepSummary {
  slug: string
  title: string
  category: CategoryId
  excerpt: string
  length: number
}

// 辅助函数：根据 ID 获取分类元信息
export function getCategoryById(id: CategoryId): CategoryMeta | undefined {
  return CATEGORIES.find(c => c.id === id)
}

// 辅助函数：获取某分类的所有文章
export function getArticlesByCategory(articles: LearnDeepSummary[], categoryId: CategoryId): LearnDeepSummary[] {
  return articles.filter(a => a.category === categoryId)
}

// 按分类统计
export function getLearnDeepStats() {
  return CATEGORIES.map(c => ({
    id: c.id,
    name: c.name,
    count: c.count
  }))
}

// 按分类分组的文章列表
export function getLearnDeepByCategory() {
  const result: Record<CategoryId, LearnDeepSummary[]> = {} as Record<CategoryId, LearnDeepSummary[]>
  for (const cat of CATEGORIES) {
    result[cat.id] = learnDeepList.filter(a => a.category === cat.id)
  }
  return result
}

`;

// 在 export const learnDeepList 之前插入分类定义
const newContent = beforeArr + categoryDefs + newArrText + '\n' + afterArr;

// 修复数组语法：learnDeepList = [ ... ] 后需要 '];' 闭合
const newContentFixed = newContent.replace(/(\n\]\])\s*$/, '];\n\n// 初始化分类计数\nfor (const cat of CATEGORIES) {\n  cat.count = learnDeepList.filter(a => a.category === cat.id).length\n}\n');

fs.writeFileSync(filePath, newContentFixed, 'utf-8');

console.log('\n✅ 分类迁移完成！');
console.log('文件已更新:', filePath);