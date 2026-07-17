/**
 * classify-items.js
 *
 * 对 300 个知识点进行"深入度"三级分类。
 * 决定每篇 learn-deep 文章的篇幅和深度：
 * - Strong: 核心基础知识点 → 完整深究文章
 * - Medium: 重要但范围窄的知识点 → 简洁深入文章
 * - Weak: 工具/流程类知识点 → 简短说明
 *
 * 用法: node scripts/classify-items.js
 * 输出: scripts/data/classification-output.json
 */

const fs = require('fs');
const path = require('path');

const itemsPath = path.resolve(__dirname, 'data', 'items-registry.json');
const { items } = JSON.parse(fs.readFileSync(itemsPath, 'utf-8'));

// ==========================================
// Weak: 工具/流程/配置类知识点
// ==========================================
const WEAK_IDS = [
  'syntax', 'control_flow', 'gdb', 'makefile', 'git',
  'valgrind', 'static_analysis', 'unit_test', 'code_style',
  'secure_coding', 'cross_platform', 'ci',
  'cpp-solid', 'cmake', 'gdb_cpp', 'git_cpp', 'cpp-clang-tidy',
  'cpp-gtest', 'cpp-sanitizers', 'cpp-package-mgr', 'docker',
  'cpp-ci-cd', 'cpp-code-review',
  'sort_basic_ds', 'ds-game-theory', 'ds-big-data', 'ds-online-algo',
  'py-venv', 'py-db-access', 'py-pytest', 'git_py', 'py-debug',
  'py-web', 'py-code-quality', 'docker_py', 'py-celery', 'py-grpc',
  'ci_py', 'py-fileio', 'py-model-deploy',
  'linux-fio', 'linux-fs-journal',
];

function isWeak(item) {
  if (WEAK_IDS.includes(item.id)) return true;
  const weakTagHit = item.tags.some(t =>
    ['Git', '版本控制', 'CI/CD', '包管理', 'vcpkg',
     'Docker', '部署', '单元测试', '静态分析', 'lint',
     '编码规范', '风格', '基准测试'].includes(t)
  );
  return weakTagHit;
}

// ==========================================
// Strong: 核心/基础知识点
// ==========================================
const STRONG_IDS = [
  // C 核心
  'pointer', 'dynamic_memory', 'struct_union', 'array_string',
  'function_scope', 'func_pointer', 'c-memory-layout', 'file_io',
  'c-advanced-pointer', 'c-const-volatile', 'c-compiler-optimization',
  'c-linker', 'preprocessor', 'c-macro', 'pthread',
  'process', 'signal', 'ipc',
  // C++ 核心
  'class_object', 'inheritance', 'cpp-templates', 'raii',
  'smart_ptr', 'cpp-move-semantics', 'stl', 'exception',
  'cpp-memory-model', 'cpp-thread', 'cpp-memory-pool',
  'cpp-allocator', 'cpp-lambda', 'cpp-coroutines',
  // DS 核心
  'array', 'ds-linkedlist', 'ds-stack', 'ds-queue', 'ds-hashtable',
  'ds-binarytree', 'ds-bst', 'ds-avl', 'ds-rbtree', 'ds-btree',
  'ds-heap', 'ds-trie', 'ds-graph-repr', 'ds-bfs-dfs',
  'ds-dijkstra', 'ds-mst', 'ds-bit', 'ds-union-find',
  'dp_ds', 'ds-recursion', 'sort_advanced_ds', 'ds-binsearch',
  'string_match_ds', 'ds-recursion',
  // DB 核心
  'storage_overview', 'db-page-block', 'row_format', 'db-columnar',
  'buffer_pool', 'db-wal', 'db-compaction', 'db-lsm',
  'db-sstable', 'db-btree-idx', 'db-btree-impl', 'db-hash-idx',
  'db-fulltext', 'db-spatial',
  'db-vector-basic', 'db-ivf-variants', 'db-hnsw', 'db-graph-index',
  'db-pq-quant', 'db-quantization', 'db-scalar-quant', 'db-diskann',
  'db-hybrid-search',
  'db-optimizer', 'db-executor', 'db-executor-detail',
  'db-acid', 'db-mvcc-principle', 'db-locking', 'deadlock',
  'db-redo-log-detail', 'db-aries',
  'db-milvus-arch', 'db-milvus-segment', 'db-milvus-index', 'db-milvus-search',
  'vdb-pinecone-serverless', 'vdb-pinecone-storage',
  'db-sqlite-arch', 'db-sql-dcl',
  // PY 核心
  'basic_types', 'control_flow_py', 'py-function', 'py-oop-basic',
  'py-iterator', 'py-generator', 'py-decorator',
  'py-threading', 'py-multiprocessing', 'py-network',
  'py-numpy', 'py-pytorch', 'py-sklearn',
  'py-memory', 'py-gil', 'py-cpython',
  // Linux 核心
  'syscall', 'pagecache', 'linux-cfs',
  'linux-vm', 'linux-direct-io', 'linux-io-scheduler',
  'linux-mmap-db', 'linux-rcu', 'linux-ext4-xfs',
  'linux-block-layer', 'linux-btrfs',
  'linux-io-uring-storage', 'linux-tcp-state',
  'linux-epoll', 'linux-bridge', 'linux-ebpf-intro',
];

const STRONG_IDS_PATTERN = [
  /^cpp-(atomic|lockfree|cond-var)$/,
  /^ds-(string-ds|trie|binarytree)$/,
  /^py-(asyncio|typing)$/,
  /^linux-(pagecache-hit|hugepages|fallocate|fuse|nfs|raid|fs-cache|zero-copy|so-reuseport|socket-buf|unix-socket|namespace|cgroup2|seccomp|xdp|iptables|veth|cpu-affinity|numa-observ|db-flamegraph|latency-diag|disk-ssd|iouring)$/,
  /^db-(redis-|consensus|sharding|ha)/,
  /^ds-(topo-sort|scc|network-flow|euler-path|lis|lcs|scanline)/,
];

function isStrong(item) {
  if (WEAK_IDS.includes(item.id)) return false;
  if (STRONG_IDS.includes(item.id)) return true;
  if (STRONG_IDS_PATTERN.some(p => p.test(item.id))) return true;

  // VDB stack all strong
  if (item.stack === 'vdb') return true;

  // Tag-based: core data structure/algorithm concepts
  const coreTags = ['指针', '树', '图', '哈希', '事务', 'MVCC', '锁',
    'ACID', '并发', '同步', '原子', 'RAII',
    '向量索引', 'HNSW', 'IVF', '量化', '图索引', 'PQ',
    'PyTorch', 'NumPy', '数值计算',
    '调度', '文件系统', '虚拟内存'];
  if (item.tags.some(t => coreTags.includes(t))) return true;

  return false;
}

// ==========================================
// 执行分类（Weak 优先检查，避免工具类被错分到 Strong）
// ==========================================
const classification = {};
let strong = 0, medium = 0, weak = 0;

items.forEach(item => {
  let tier, rationale;

  if (isWeak(item)) {
    tier = 'weak';
    rationale = '工具/流程类知识点，简短说明即可';
  } else if (isStrong(item)) {
    tier = 'strong';
    rationale = '核心基础知识，值得深入讲解';
  } else {
    tier = 'medium';
    rationale = '重要但范围较窄，适合简洁深入讲解';
  }

  classification[item.id] = {
    itemId: item.id,
    stack: item.stack,
    tier,
    rationale,
    hasExistingArticle: false, // filled later
  };
  if (tier === 'strong') strong++;
  else if (tier === 'medium') medium++;
  else weak++;
});

console.log(`分类结果: Strong=${strong} Medium=${medium} Weak=${weak}\n`);

// 按栈输出分布
const byStack = {};
items.forEach(item => {
  const s = item.stack;
  if (!byStack[s]) byStack[s] = { strong: 0, medium: 0, weak: 0 };
  byStack[s][classification[item.id].tier]++;
});
Object.entries(byStack).forEach(([s, counts]) => {
  console.log(`${s.padEnd(8)} Strong=${counts.strong} Medium=${counts.medium} Weak=${counts.weak}`);
});

// 输出 weak + strong 列表供审查
console.log('\n--- Strong items ---');
items.filter(item => classification[item.id].tier === 'strong').forEach(item => {
  console.log(`  ${item.id.padEnd(30)} ${item.title}`);
});
console.log('\n--- Weak items ---');
items.filter(item => classification[item.id].tier === 'weak').forEach(item => {
  console.log(`  ${item.id.padEnd(30)} ${item.title}`);
});

// 写入文件
const outPath = path.resolve(__dirname, 'data', 'classification-output.json');
const outData = {
  total: items.length,
  strong, medium, weak,
  items: Object.values(classification)
};
fs.writeFileSync(outPath, JSON.stringify(outData, null, 2), 'utf-8');
console.log(`\n写入分类结果: ${outPath}`);
