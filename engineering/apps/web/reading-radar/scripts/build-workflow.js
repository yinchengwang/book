/**
 * build-workflow.js
 *
 * 读取所有批次 prompt 文件，生成一个自包含的 Workflow 脚本文件。
 * 之后调用 Workflow({scriptPath}) 来执行。
 *
 * 用法: node scripts/build-workflow.js
 * 输出: scripts/workflow-gen.js
 */

const fs = require('fs');
const path = require('path');

const promtsDir = path.resolve(__dirname, 'data', 'batch-prompts');
const outPath = path.resolve(__dirname, 'workflow-gen.js');

const batchFiles = fs.readdirSync(promtsDir)
  .filter(f => f.startsWith('batch-') && f.endsWith('.json'))
  .sort();

// Read all batches
const batches = batchFiles.map(f => {
  const batch = JSON.parse(fs.readFileSync(path.join(promtsDir, f), 'utf-8'));
  return {
    batchNum: batch.batchNum,
    stack: batch.stack,
    systemPrompt: batch.systemPrompt,
    userPrompt: batch.userPrompt,
  };
});

// Build workflow script as array to avoid template literal escaping issues
const lines = [
  '// Auto-generated workflow script',
  '// Total batches: ' + batches.length,
  '',
  'export const meta = {',
  "  name: 'learn-deep-dl-content',",
  "  description: '为 300 个知识点生成深度学习关联内容（21 批并行）',",
  '  phases: [',
  "    { title: '生成内容', detail: '21 批并行生成 DL 关联文章' },",
  "    { title: '收集验证', detail: '解析和验证生成结果' },",
  '  ],',
  '};',
  '',
  'const BATCHES = ' + JSON.stringify(batches, null, 2) + ';',
  '',
  "// ── Phase 1: 并行生成内容 ──",
  "phase('生成内容');",
  '',
  'const results = await pipeline(',
  '  BATCHES,',
  '  // Stage 1: 生成内容',
  '  async (batch) => {',
  "    const prompt = batch.systemPrompt + '\\n\\n' + batch.userPrompt;",
  '    return await agent(prompt, {',
  "      label: 'batch-' + String(batch.batchNum).padStart(2, '0') + ' [' + batch.stack + ']',",
  "      phase: '生成内容',",
  '    });',
  '  },',
  '  // Stage 2: 解析验证',
  '  async (output, batch) => {',
  "    // 按 <!-- item: --> 分割",
  '    const items = [];',
  "    const regex = /<!--\\s*item:\\s*([\\w-]+)\\s*-->([\\s\\S]*?)(?=\\n<!--\\s*item:|\\n---|$)/g;",
  '    let match;',
  '',
  '    while ((match = regex.exec(output)) !== null) {',
  '      items.push({',
  '        itemId: match[1].trim(),',
  '        content: match[2].trim(),',
  '      });',
  '    }',
  '',
  '    return {',
  '      batchNum: batch.batchNum,',
  '      stack: batch.stack,',
  "      expectedCount: batch.userPrompt.split('### ').length - 1,",
  '      actualCount: items.length,',
  '      items,',
  '      rawOutput: output,',
  '    };',
  '  }',
  ');',
  '',
  "// ── Phase 2: 收集验证 ──",
  "phase('收集验证');",
  '',
  'let totalItems = 0;',
  'let totalErrors = 0;',
  'const allContent = [];',
  '',
  'for (const result of results) {',
  '  if (!result) continue;',
  '  totalItems += result.actualCount;',
  '  if (result.actualCount < result.expectedCount) {',
  '    totalErrors++;',
  "    log('Batch ' + result.batchNum + ': expected ' + result.expectedCount + ' items, got ' + result.actualCount);",
  '  }',
  '  for (const item of (result.items || [])) {',
  '    allContent.push({',
  '      itemId: item.itemId,',
  '      content: item.content,',
  '      batchNum: result.batchNum,',
  '      stack: result.stack,',
  '    });',
  '  }',
  '}',
  '',
  "log('Total: ' + totalItems + ' items generated, ' + totalErrors + ' batches with errors');",
  '',
  'return {',
  '  totalBatches: results.length,',
  '  totalItems,',
  '  errorBatches: totalErrors,',
  '  allContent,',
  '};',
];

const script = lines.join('\n');

fs.writeFileSync(outPath, script, 'utf-8');
console.log('Workflow 脚本已生成: ' + outPath);
console.log('文件大小: ' + (fs.statSync(outPath).size / 1024).toFixed(1) + ' KB');
console.log('包含 ' + batches.length + ' 批数据');
