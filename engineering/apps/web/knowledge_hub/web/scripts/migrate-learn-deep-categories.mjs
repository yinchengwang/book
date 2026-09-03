/**
 * 图解系列分类迁移脚本 - 修复版
 */

import { readFileSync, writeFileSync } from 'fs';
import { join } from 'path';

const filePath = join(__dirname, '../../src/data/learn_deep_index.ts');
let content = readFileSync(filePath, 'utf-8');

// 分类关键词映射表
const categoryRules = [
  { kws: ['向量', 'hnsw', 'ivf', 'diskann', 'milvus', 'pinecone', 'faiss', 'chroma', 'pgvector', 'weaviate', 'qdrant', '量化', '近似最近邻', 'lance', 'vearch', 'proxima'], cat: 'vdb' },
  { kws: ['linux', 'page cache', 'procfs', 'perf', 'blktrace', 'htop', 'sar', 'io 调度', 'cfs', 'mmap', 'cgroup', 'io_uring', 'ext4', 'xfs', 'btrfs', 'nfs', 'lvm', 'raid', '块设备', 'fio', 'fuse', 'fsync', 'strace', 'seccomp', 'ebpf', 'hugepages', 'direct io', 'oom killer', '火焰图', 'io 追踪', 'procfs', '网桥', '网络命名空间', 'cfs'], cat: 'linux' },
  { kws: ['tcp', 'socket', 'http', 'https', 'ssl', 'tls', 'epoll', 'reactor', 'proactor', '零拷贝', 'xdp', 'iptables', '连接池', 'backlog', '三次握手', '四次挥手', '重传', 'udp', 'websocket', 'quic', 'rpc', 'dns', 'cdn', 'nginx', 'veth', 'bridge', 'unix domain', 'tcp状态机', 'tcpdump', 'so_reuseport'], cat: 'network' },
  { kws: ['cpu缓存', '预读', '分支预测', '中断', '软中断', '锁', 'mutex', '自旋锁', 'rcu', '内存屏障', '虚拟内存', '进程', '线程', '调度', '上下文切换', 'numa', 'oom', '内存泄漏', 'malloc', 'glibc', '内核', '死锁', '一致性', '浮点数', '键盘敲入', '进程最多', '调度器', '时间片', '4gb', '文件全家桶', '全家桶', 'ping', '磁盘比内存', 'syn', '丢弃'], cat: 'system' },
];

function classify(title) {
  const t = title.toLowerCase();
  for (const rule of categoryRules) {
    for (const kw of rule.kws) {
      if (t.includes(kw)) return rule.cat;
    }
  }
  return null;
}

// 统计
const stats = { linux: 0, network: 0, system: 0, database: 0, vdb: 0, algorithm: 0, c: 0, python: 0, cpp: 0 };

// 处理所有 category 字段
// 正则匹配整个 category 行
const catLineRegex = /"category":\s*"([^"]+)"/g;
let match;
let replacedCount = 0;

while ((match = catLineRegex.exec(content)) !== null) {
  const fullMatch = match[0];
  const oldCat = match[1];

  // 跳过已处理的中文分类（算法、C、Python、C++）
  if (oldCat === '算法') {
    content = content.replace(fullMatch, '"category": "algorithm"');
    continue;
  }
  if (oldCat === 'C') {
    content = content.replace(fullMatch, '"category": "c"');
    continue;
  }
  if (oldCat === 'Python') {
    content = content.replace(fullMatch, '"category": "python"');
    continue;
  }
  if (oldCat === 'C++') {
    content = content.replace(fullMatch, '"category": "cpp"');
    continue;
  }

  // 对于"综合"和"数据库"，需要找到对应的 title
  if (oldCat === '综合' || oldCat === '数据库') {
    // 找到这个 category 行之前的 title
    const beforeText = content.substring(0, match.index);
    const lastTitleMatch = beforeText.lastIndexOf('"title": "');
    const titleStart = lastTitleMatch + 10;
    const titleEnd = content.indexOf('"', titleStart);
    const title = content.substring(titleStart, titleEnd);

    const newCat = classify(title) || (oldCat === '综合' ? 'system' : 'database');

    if (newCat !== oldCat) {
      content = content.replace(fullMatch, `"category": "${newCat}"`);
      stats[newCat]++;
      replacedCount++;
    } else {
      content = content.replace(fullMatch, `"category": "${newCat}"`);
      stats[newCat]++;
    }
  }
}

// 统计
for (const cat of ['algorithm', 'c', 'python', 'cpp']) {
  const count = (content.match(new RegExp(`"category": "${cat}"`, 'g')) || []).length;
  stats[cat] = count;
}

// 输出统计
console.log('\n分类统计：');
const total = Object.values(stats).reduce((a, b) => a + b, 0);
for (const [cat, count] of Object.entries(stats)) {
  console.log(`  ${cat}: ${count}`);
}
console.log(`  Total: ${total}`);
console.log(`  替换: ${replacedCount}`);

// 更新 LearnDeepSummary 接口
content = content.replace(
  'export interface LearnDeepSummary {',
  `export const CATEGORY_ORDER = ['linux', 'network', 'system', 'database', 'vdb', 'algorithm', 'c', 'python', 'cpp'] as const

export type CategoryId = typeof CATEGORY_ORDER[number]

export interface CategoryMeta {
  id: CategoryId
  name: string
  slug: string
}

export const CATEGORIES: CategoryMeta[] = [
  { id: 'linux', name: 'Linux', slug: 'linux' },
  { id: 'network', name: '网络', slug: 'network' },
  { id: 'system', name: '系统', slug: 'system' },
  { id: 'database', name: '数据库', slug: 'database' },
  { id: 'vdb', name: '向量数据库', slug: 'vdb' },
  { id: 'algorithm', name: '算法', slug: 'algorithm' },
  { id: 'c', name: 'C', slug: 'c' },
  { id: 'python', name: 'Python', slug: 'python' },
  { id: 'cpp', name: 'C++', slug: 'cpp' },
]

export function getCategoryById(id: CategoryId): CategoryMeta | undefined {
  return CATEGORIES.find(c => c.id === id)
}

export function getArticlesByCategory(articles: LearnDeepSummary[], categoryId: CategoryId): LearnDeepSummary[] {
  return articles.filter(a => a.category === categoryId)
}

export function getLearnDeepStats() {
  return CATEGORIES.map(c => ({
    id: c.id,
    name: c.name,
    count: learnDeepList.filter(a => a.category === c.id).length
  }))
}

export function getLearnDeepByCategory() {
  const result: Partial<Record<CategoryId, LearnDeepSummary[]>> = {}
  for (const cat of CATEGORIES) {
    result[cat.id] = learnDeepList.filter(a => a.category === cat.id)
  }
  return result as Record<CategoryId, LearnDeepSummary[]>
}

export interface LearnDeepSummary {`
);

content = content.replace(
  /  category: string\n/,
  '  category: CategoryId\n'
);

writeFileSync(filePath, content, 'utf-8');

console.log('\n✅ 分类迁移完成！');
console.log('文件已更新:', filePath);