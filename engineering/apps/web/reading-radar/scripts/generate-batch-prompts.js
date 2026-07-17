/**
 * generate-batch-prompts.js
 *
 * 从分类结果生成 20 批的 prompt 文件，每批包含 ~15 个知识点。
 * 按技术栈分组保持主题连贯。
 *
 * 用法: node scripts/generate-batch-prompts.js
 * 输出: scripts/data/batch-prompts/batch-XX.json
 */

const fs = require('fs');
const path = require('path');

const classPath = path.resolve(__dirname, 'data', 'classification-output.json');
const classification = JSON.parse(fs.readFileSync(classPath, 'utf-8'));

const outDir = path.resolve(__dirname, 'data', 'batch-prompts');
if (!fs.existsSync(outDir)) fs.mkdirSync(outDir, { recursive: true });

// ==========================================
// 分批策略
// ==========================================
// 每批保持同栈，~15 个/批
const STACK_ORDER = ['c', 'cpp', 'ds', 'db', 'vdb', 'py', 'linux'];
const BATCH_CONFIG = [
  // [stack, start, end] — items grouped contiguously within same stack
  { stack: 'c',    batchNum: 1,  start: 0,  end: 14 },  // 15 items
  { stack: 'c',    batchNum: 2,  start: 15, end: 29 },  // 15
  { stack: 'c',    batchNum: 3,  start: 30, end: 43 },  // 14
  { stack: 'cpp',  batchNum: 4,  start: 0,  end: 13 },  // 14
  { stack: 'cpp',  batchNum: 5,  start: 14, end: 27 },  // 14
  { stack: 'cpp',  batchNum: 6,  start: 28, end: 41 },  // 14
  { stack: 'ds',   batchNum: 7,  start: 0,  end: 12 },  // 13
  { stack: 'ds',   batchNum: 8,  start: 13, end: 25 },  // 13
  { stack: 'ds',   batchNum: 9,  start: 26, end: 38 },  // 13
  { stack: 'ds',   batchNum: 10, start: 39, end: 51 },  // 13
  { stack: 'db',   batchNum: 11, start: 0,  end: 14 },  // 15
  { stack: 'db',   batchNum: 12, start: 15, end: 29 },  // 15
  { stack: 'db',   batchNum: 13, start: 30, end: 47 },  // 18
  { stack: 'vdb',  batchNum: 14, start: 0,  end: 14 },  // 15
  { stack: 'py',   batchNum: 15, start: 0,  end: 13 },  // 14
  { stack: 'py',   batchNum: 16, start: 14, end: 28 },  // 15
  { stack: 'py',   batchNum: 17, start: 29, end: 42 },  // 14
  { stack: 'linux', batchNum: 18, start: 0,  end: 13 }, // 14
  { stack: 'linux', batchNum: 19, start: 14, end: 27 }, // 14
  { stack: 'linux', batchNum: 20, start: 28, end: 41 }, // 14
  { stack: 'linux', batchNum: 21, start: 42, end: 55 }, // 14
];

// ==========================================
// 模板定义
// ==========================================
const TIER_TEMPLATES = {
  strong: `- 要求写一篇完整、深入的讲解文章，涵盖以下方面（可根据知识点灵活调整）：
  - 核心概念与基本原理
  - 内部实现机制
  - 在主流框架/系统/数据库中的具体实现差异（如适用）
  - 性能特征与优化技巧
  - 代码示例（真实代码，非伪代码）
  - 常见陷阱与最佳实践
  - 延伸阅读链接（2-3个相关learn-deep知识点ID）
- 篇幅：约 800-1500 tokens`,

  medium: `- 要求写一篇简洁但深入的讲解文章，涵盖以下方面：
  - 核心概念与基本原理
  - 关键实现细节
  - 实际应用场景
  - 延伸阅读链接（1-2个相关learn-deep知识点ID）
- 篇幅：约 400-600 tokens`,

  weak: `- 要求写一篇简短但言之有物的讲解段落：
  - 核心概念一句话概括
  - 关键要点
  - 延伸阅读链接（1个相关learn-deep知识点ID）
- 篇幅：约 100-200 tokens`
};

const SUPPLEMENT_TEMPLATES = {
  strong: `- 要求写一篇补充章节（追加到已有文章末尾），为该知识点补充之前未覆盖的深度内容：
  - 可以是新的实现视角、新的应用场景
  - 或者更深入的内幕细节
  - 避免与已有内容重复
  - 约 400-800 tokens`,

  medium: `- 要求写一段补充内容（追加到已有文章末尾）：
  - 一个额外的关键细节或视角
  - 约 200-400 tokens`,

  weak: `- 要求写一段简短的补充（追加到已有文章末尾）：
  - 一个实用提示或冷知识
  - 约 50-150 tokens`
};

// ==========================================
// 构建系统提示
// ==========================================
function buildSystemPrompt(stack) {
  const IDENTITIES = {
    c:     '你是一名资深 C 语言技术专家和 C 语言培训讲师',
    cpp:   '你是一名资深 C++ 技术专家和 C++ 培训讲师',
    ds:    '你是一名资深数据结构和算法工程师、算法竞赛教练',
    db:    '你是一名资深数据库内核开发工程师和数据库技术布道者',
    vdb:   '你是一名向量数据库技术专家和数据库索引工程师',
    py:    '你是一名资深 Python 全栈开发者和 Python 技术布道者',
    linux: '你是一名 Linux 内核/系统工程师和系统性能优化专家',
  };
  const TASKS = {
    c:     '为下面的 C 语言知识点各写一篇深入的技术讲解文章。站在 C 语言本身的角度，深入讲解该知识点的核心概念、底层原理、最佳实践和常见陷阱',
    cpp:   '为下面的 C++ 知识点各写一篇深入的技术讲解文章。站在 C++ 本身的角度，深入讲解该知识点的核心机制、设计理念、工程实践和常见误区',
    ds:    '为下面的数据结构知识点各写一篇深入的技术讲解文章。站在该数据结构本身的角度，深入讲解其原理、实现、复杂度分析和典型应用场景',
    db:    '为下面的数据库知识点各写一篇深入的技术讲解文章。站在数据库内核的角度，深入讲解该机制的实现原理、工程设计和在主流数据库中的具体落地',
    vdb:   '为下面的向量数据库知识点各写一篇深入的技术讲解文章。站在向量数据库的角度，深入讲解该索引/机制的算法原理、工程实现和在主流系统中的应用',
    py:    '为下面的 Python 知识点各写一篇深入的技术讲解文章。站在 Python 本身的角度，深入讲解该知识点的机制原理、最佳实践和底层实现',
    linux: '为下面的 Linux 知识点各写一篇深入的技术讲解文章。站在 Linux 内核/系统的角度，深入讲解该机制的实现原理、系统调用路径和性能调优实践',
  };

  const identity = IDENTITIES[stack] || '你是一名资深软件工程师和技术布道者';
  const task = TASKS[stack] || '为下面的知识点各写一篇深入的技术讲解文章';

  return `你的身份：${identity}。
你的任务：${task}

## 输出格式要求
每条输出用以下格式包裹：

<!-- item: <itemId> -->
# 文章标题

文章正文...

---

即每条以 \`<!-- item: <itemId> -->\` 开头，内容之间用 \`---\` 分隔。

## 写作原则
1. 深入讲解知识点本身的核心概念和实现原理
2. 内容务必具体、技术准确，给出真实的代码示例和实现细节
3. 不编造不存在的研究或框架
4. 代码示例需简洁可读，用 C/C++/Python 等真实语言编写
5. 如果知识点已有文章，生成的内容是追加到现有文章末尾的补充章节
6. 延伸阅读使用格式 \`- [文章标题](知识点的itemId)\`

## 写作风格
用你对这个领域的深入了解，写出一篇有深度的技术文章，而不是泛泛的介绍。要具体到源码片段、算法步骤、工程实践。让读者看完后真正理解这个知识点的精髓。`;
}

// ==========================================
// 构建每批的 prompt
// ==========================================
function buildBatchPrompt(batch, items) {
  const { stack, batchNum } = batch;

  const itemBlocks = items.map(item => {
    const cls = classification.items.find(c => c.itemId === item.id);
    const tier = cls ? cls.tier : 'medium';
    const hasExisting = cls ? cls.hasExistingArticle : false;

    let tierTemplate;
    if (hasExisting) {
      tierTemplate = SUPPLEMENT_TEMPLATES[tier] || SUPPLEMENT_TEMPLATES.medium;
    } else {
      tierTemplate = TIER_TEMPLATES[tier] || TIER_TEMPLATES.medium;
    }

    return `### ${item.id}
- **标题**: ${item.title}
- **描述**: ${item.desc}
- **标签**: ${(item.tags || []).join(', ')}
- **深入度**: ${tier}
- **已有文章**: ${hasExisting ? '是（生成补充章节）' : '否（生成完整文章）'}
- **模板**:
${tierTemplate}
`;
  }).join('\n');

  return {
    batchNum,
    stack,
    systemPrompt: buildSystemPrompt(stack),
    userPrompt: `## 技术栈: ${stack}

以下是为您分配的 ${items.length} 个知识点。请为每个知识点生成一篇深入的技术讲解文章。

${itemBlocks}

---
请严格按照 \`<!-- item: <itemId> -->\` 格式输出，每条之间用 \`---\` 分隔。
`,
    items: items.map(i => ({ id: i.id, title: i.title, stack: i.stack, tier: classification.items.find(c => c.itemId === i.id)?.tier || 'medium', hasExistingArticle: classification.items.find(c => c.itemId === i.id)?.hasExistingArticle || false }))
  };
}

// ==========================================
// 加载 items 并按栈分组
// ==========================================
const itemsPath = path.resolve(__dirname, 'data', 'items-registry.json');
const allItems = JSON.parse(fs.readFileSync(itemsPath, 'utf-8')).items;

// Group items by stack maintaining order
const stackItems = {};
STACK_ORDER.forEach(s => { stackItems[s] = []; });
allItems.forEach(item => {
  if (stackItems[item.stack]) stackItems[item.stack].push(item);
});

// ==========================================
// 生成批次
// ==========================================
const batches = BATCH_CONFIG.map(cfg => {
  const items = stackItems[cfg.stack].slice(cfg.start, cfg.end + 1);
  const prompt = buildBatchPrompt(cfg, items);
  const outFile = path.join(outDir, `batch-${String(cfg.batchNum).padStart(2, '0')}.json`);
  fs.writeFileSync(outFile, JSON.stringify(prompt, null, 2), 'utf-8');
  return prompt;
});

console.log(`共生成 ${batches.length} 批 prompt 文件:`);
let strong = 0, medium = 0, weak = 0, supplement = 0, totalItems = 0;
batches.forEach(b => {
  const s = b.items.filter(i => i.tier === 'strong').length;
  const m = b.items.filter(i => i.tier === 'medium').length;
  const w = b.items.filter(i => i.tier === 'weak').length;
  const sup = b.items.filter(i => i.hasExistingArticle).length;
  console.log(`  批 #${String(b.batchNum).padStart(2, '0')} [${b.stack.padEnd(6)}] ${b.items.length}项  S=${s} M=${m} W=${w} 补充=${sup}`);
  strong += s; medium += m; weak += w; supplement += sup; totalItems += b.items.length;
});
console.log(`\n总计: ${totalItems} 项 (Strong=${strong} Medium=${medium} Weak=${weak} 需补充=${supplement})`);
