## 1. 基础设施与数据准备

- [ ] 1.1 编写 Node.js 脚本：从 `data/app/items-registry.js` 提取所有 300 个知识点的 id/stack/quadrant/ring/title/desc/tags 为可用的 JSON 格式
- [ ] 1.2 编写 Node.js 脚本：解析现有 `data/app/learn-deep-bundle.js` 提取已有文章的 itemId 列表（93 篇），输出 JSON
- [ ] 1.3 确认 `data/learn-deep/` 下所有 `.md` 文件的完整路径映射（处理子目录 linux/db/vdb/ 和根目录的混合情况）

## 2. Phase 1：DL 关联度分类

- [ ] 2.1 编写 Workflow 脚本 `learn-deep-workflow.js`，包含 meta 定义和三个 Phase 的骨架
- [ ] 2.2 Phase 1：启动 1 个子 agent，输入全部 300 个知识点定义 + 三级分类标准，输出分类 JSON 到 `openspec/changes/learn-deep-dl-content/classification-output.json`
- [ ] 2.3 人工 review 分类结果，修正 ~5-10% 的边界案例，确认最终分类

## 3. Phase 2：分批内容生成

- [ ] 3.1 Phase 2 主逻辑：读取分类 JSON，按技术栈分批（每批 ~15 个），生成 20 批的子 agent prompt
- [ ] 3.2 子 agent Prompt 模板：包含系统指令、上下文、范例文章、三级模板要求、`<!-- item: -->` 输出标记规范
- [ ] 3.3 每批启动子 agent 生成内容，收集所有批次输出
- [ ] 3.4 解析子 agent 输出：按 `<!-- item: -->` 分割，验证每批完整性（item 数量匹配、无遗漏），输出结构化 JSON（`Map<itemId, markdownContent>`）

## 4. Phase 2（补充策略）：已有文章的 DL 章节补充

- [ ] 4.1 从 Phase 2 输出中筛选 `hasExistingArticle: true` 的条目
- [ ] 4.2 解析子 agent 对这些条目的输出，提取要追加的 DL 章节内容
- [ ] 4.3 验证补充内容不与原文已有 DL 相关部分重复

## 5. Phase 3：集成与文件写入

- [ ] 5.1 备份阶段：创建 `data/learn-deep/.backup/` 目录，将 93 篇已有文章全量备份（`<item-id>.md.original`）
- [ ] 5.2 新建文章阶段：遍历 ~207 个无已有文章的知识点，在 `data/learn-deep/<itemId>.md` 写入子 agent 生成的内容
- [ ] 5.3 追加阶段：遍历 93 篇已有文章，在文件末尾追加 DL 章节内容
- [ ] 5.4 Bundle 重建：扫描 `data/learn-deep/` 下所有 `.md` 文件，重建 `data/app/learn-deep-bundle.js`
- [ ] 5.5 验证覆盖：确认 `LEARN_DEEP_CONTENT` 对象有 300 个非空条目

## 6. 验证与修复

- [ ] 6.1 覆盖率验证脚本：加载 `learn-deep-bundle.js`，确认 300/300 条都存在且非空
- [ ] 6.2 Sample 验证：随机抽查 5 篇强关联文章，确认结构完整（四节）、代码示例合理
- [ ] 6.3 Sample 验证：随机抽查 3 篇补充文章（已有文章的追加），确认原文未被破坏
- [ ] 6.4 Git 状态确认：只有 `data/learn-deep/` 和 `data/app/learn-deep-bundle.js` 被改动
- [ ] 6.5 启动 `node server.js`，在浏览器验证 learn.html 中随机 3 个知识点的深度文章正常渲染

## 7. 收尾

- [ ] 7.1 如果有手动修正的内容，重新重建 bundle 确保一致性
- [ ] 7.2 Git commit：提交 message 为 "feat: 为全部 300 个知识点补充深度学习关联内容"
- [ ] 7.3 提交 PR 或直接 push
