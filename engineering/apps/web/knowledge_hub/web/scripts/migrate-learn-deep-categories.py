#!/usr/bin/env python3
"""
图解系列分类迁移脚本
使用 Python 解析伪 JSON 数据
"""

import re
import json
import sys

# 读取文件
file_path = '../../src/data/learn_deep_index.ts'
with open(file_path, 'r', encoding='utf-8') as f:
    content = f.read()

# 提取数组内容
arr_match = re.search(r'export const learnDeepList: LearnDeepSummary\[\] = \[(.*)\];', content, re.DOTALL)
if not arr_match:
    print('无法找到 learnDeepList 数组')
    sys.exit(1)

arr_text = arr_match.group(1)

# 使用更简单的方法：直接用正则提取每篇文章的字段
# 每篇文章格式：{ "slug": "...", "title": "...", "category": "...", "excerpt": "...", "length": N }

# 匹配整个对象（处理嵌套的花括号）
def parse_articles(text):
    articles = []
    i = 0
    n = len(text)

    while i < n:
        # 跳过空白和逗号
        while i < n and text[i] in ' \t\n\r,':
            i += 1
        if i >= n:
            break

        if text[i] != '{':
            i += 1
            continue

        # 找匹配的 }
        depth = 1
        j = i + 1
        in_string = False
        string_char = ''

        while j < n and depth > 0:
            c = text[j]
            if not in_string:
                if c == '"':
                    in_string = True
                    string_char = '"'
                elif c == '{':
                    depth += 1
                elif c == '}':
                    depth -= 1
            else:
                if c == string_char and (j == 0 or text[j-1] != '\\'):
                    in_string = False
            j += 1

        if depth == 0:
            entry_text = text[i:j]
            articles.append(entry_text)
        i = j

    return articles

articles = parse_articles(arr_text)
print(f'解析到 {len(articles)} 篇文章')

# 分类函数
def classify_article(title, slug, current_category):
    title_lower = title.lower()

    # 向量数据库关键词（优先）
    vdb_keywords = ['向量', 'hnsw', 'ivf', 'diskann', 'milvus', 'pinecone', 'faiss', 'chroma',
        'pgvector', 'weaviate', 'qdrant', '量化', '近似最近邻', 'lance', 'vearch', 'proxima']

    # Linux 关键词
    linux_keywords = ['linux', 'page cache', 'procfs', 'perf', 'blktrace', 'htop', 'sar',
        'io 调度', 'cfs', 'mmap', 'cgroup', 'io_uring', 'ext4', 'xfs', 'btrfs', 'nfs',
        'lvm', 'raid', '块设备', 'fio', 'fuse', 'fsync', 'strace', 'seccomp', 'ebpf',
        'hugepages', 'direct io', 'oom killer', 'flamegraph', '火焰图', 'io 追踪']

    # 网络关键词
    network_keywords = ['tcp', 'socket', 'http', 'https', 'ssl', 'tls', 'epoll',
        'reactor', 'proactor', '零拷贝', 'xdp', 'iptables', '连接池', 'backlog',
        '三次握手', '四次挥手', '重传', 'udp', 'websocket', 'quic', 'rpc', 'dns',
        'cdn', 'nginx', 'veth', 'bridge', 'unix domain', 'tcp状态机', 'tcpdump']

    # 系统关键词
    system_keywords = ['cpu缓存', '预读', '分支预测', '中断', '软中断', '锁', 'lock',
        'mutex', '自旋锁', 'rcu', '内存屏障', '虚拟内存', '进程', '线程', '调度',
        '上下文切换', 'numa', 'oom', '内存泄漏', 'malloc', 'glibc', '内核',
        '死锁', '一致性', '浮点数', '键盘敲入', '进程最多', '调度器']

    # 优先向量数据库
    for kw in vdb_keywords:
        if kw in title_lower:
            return 'vdb'

    for kw in linux_keywords:
        if kw in title_lower:
            return 'linux'

    for kw in network_keywords:
        if kw in title_lower:
            return 'network'

    for kw in system_keywords:
        if kw in title_lower:
            return 'system'

    # 默认分类
    if current_category == '综合':
        return 'system'
    if current_category == '数据库':
        return 'database'

    # 保持不变的分类
    mapping = {'算法': 'algorithm', 'C': 'c', 'Python': 'python', 'C++': 'cpp'}
    return mapping.get(current_category, current_category)

# 处理每篇文章
category_count = {cat: 0 for cat in ['linux', 'network', 'system', 'database', 'vdb', 'algorithm', 'c', 'python', 'cpp']}

def extract_field(text, field):
    """提取字段值"""
    pattern = rf'"{field}":\s*"([^"\\]*(?:\\.[^"\\]*)*)"'
    match = re.search(pattern, text)
    if match:
        # 处理转义
        return match.group(1).encode().decode('unicode_escape')
    return None

new_articles = []
for article_text in articles:
    slug = extract_field(article_text, 'slug')
    title = extract_field(article_text, 'title')
    category = extract_field(article_text, 'category')

    if not slug or not title or not category:
        print(f'无法解析: {article_text[:100]}')
        new_articles.append(article_text)
        continue

    new_category = classify_article(title, slug, category)
    category_count[new_category] += 1

    # 替换 category 字段
    new_text = re.sub(
        r'"category":\s*"[^"]*"',
        f'"category": "{new_category}"',
        article_text
    )
    new_articles.append(new_text)

# 输出统计
print('\n分类统计：')
for cat, count in category_count.items():
    print(f'  {cat}: {count}')
total = sum(category_count.values())
print(f'  Total: {total}')

# 构建新内容
category_defs = '''
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

'''

# 构建新文件
before_arr = content[:arr_match.start()]
after_arr = content[arr_match.end():]

new_content = before_arr + 'export const learnDeepList: LearnDeepSummary[] = [\n  ' + \
              ',\n  '.join(new_articles) + '\n' + '];' + category_defs

# 添加初始化计数
new_content += '''
// 初始化分类计数
for (const cat of CATEGORIES) {
  cat.count = learnDeepList.filter(a => a.category === cat.id).length
}
'''

# 写回文件
with open(file_path, 'w', encoding='utf-8') as f:
    f.write(new_content)

print(f'\n✅ 分类迁移完成！')
print(f'文件已更新: {file_path}')