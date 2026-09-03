export const meta = {
  name: 'rewrite-batch4',
  description: '批量改写 C 语言系统编程文件',
  phases: [{ title: '改写', detail: '10 个文件并行改写' }],
}

// 统一的改写风格模板
const STYLE_TEMPLATE = (topic, content) => `# ${topic}

${content}

---

## 面试高频追问

> **Q**: 常见问题1？
> **A**: 简洁回答。

> **Q**: 常见问题2？
> **A**: 简洁回答。

> **Q**: 常见问题3？
> **A**: 简洁回答。

## 代码示例

\`\`\`c
// 精简可运行的代码
\`\`\`

> **一句话总结**：${topic}的核心就是一句话概括。
`

// 批 4 文件列表
const batch4Files = [
  { path: 'c:\\\\code\\\\book\\\\project\\\\reading-radar\\\\data\\\\learn-deep\\\\c\\\\systems\\\\pthread.md', topic: 'POSIX线程(pthread)' },
  { path: 'c:\\\\code\\\\book\\\\project\\\\reading-radar\\\\data\\\\learn-deep\\\\c\\\\systems\\\\signal.md', topic: '信号处理' },
  { path: 'c:\\\\code\\\\book\\\\project\\\\reading-radar\\\\data\\\\learn-deep\\\\c\\\\systems\\\\ipc.md', topic: '进程间通信(IPC)' },
  { path: 'c:\\\\code\\\\book\\\\project\\\\reading-radar\\\\data\\\\learn-deep\\\\c\\\\systems\\\\file_io.md', topic: '文件I/O与标准IO' },
  { path: 'c:\\\\code\\\\book\\\\project\\\\reading-radar\\\\data\\\\learn-deep\\\\c\\\\systems\\\\io_multiplex.md', topic: 'I/O多路复用' },
  { path: 'c:\\\\code\\\\book\\\\project\\\\reading-radar\\\\data\\\\learn-deep\\\\c\\\\systems\\\\mmap.md', topic: '内存映射(mmap)' },
  { path: 'c:\\\\code\\\\book\\\\project\\\\reading-radar\\\\data\\\\learn-deep\\\\c\\\\systems\\\\daemon.md', topic: '守护进程与系统服务' },
  { path: 'c:\\\\code\\\\book\\\\project\\\\reading-radar\\\\data\\\\learn-deep\\\\c\\\\systems\\\\error_handling.md', topic: '错误处理与errno' },
  { path: 'c:\\\\code\\\\book\\\\project\\\\reading-radar\\\\data\\\\learn-deep\\\\c\\\\systems\\\\stdlib.md', topic: 'C标准库' },
  { path: 'c:\\\\code\\\\book\\\\project\\\\reading-radar\\\\data\\\\learn-deep\\\\c\\\\systems\\\\lkm.md', topic: 'Linux内核模块基础' },
]

// 为每个文件生成改写 prompt
const prompts = batch4Files.map(f => ({
  path: f.path,
  topic: f.topic,
  prompt: \`请将以下 Markdown 文件改写为"说人话"风格。

改写风格要求：
1. 人话开场白：一句话解释概念本质，用生活类比
2. ASCII 图解：用 text 代码块画直观示意图或对比表
3. 对比表格：多方案对比，说清优缺点
4. 面试追问：5-6 个高频问题，简洁回答
5. 代码示例：精简可运行的 C 代码演示核心逻辑
6. 一句话总结：收尾

保持原有技术准确性，改写是"翻译风格"不是"改内容"。
删除冗余重复的补充内容。

文件路径：${f.path}
主题：${f.topic}

以下是原文内容：
\`\`\`
ORIGINAL_CONTENT_HERE
\`\`\`

请输出改写后的完整 Markdown 内容。\`
}))

// 由于 workflow 不能直接读取文件，我们改用 phase + agent 模式
// 这里定义每个 agent 的任务

phase('改写')
log('开始批 4 改写：C 语言系统编程 10 个文件')

// 由于 workflow script 的限制，这里只记录元数据
// 实际改写需要通过 Agent 工具逐个进行

log('批 4 文件列表：')
for (const f of batch4Files) {
  log(\`  - \${f.topic} (\${f.path})\`)
}

return { batch: 4, count: batch4Files.length, files: batch4Files.map(f => f.path) }
