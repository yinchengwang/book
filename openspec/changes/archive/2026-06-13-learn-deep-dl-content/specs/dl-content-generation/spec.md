## ADDED Requirements

### Requirement: Tiered article templates
The system SHALL define three article templates corresponding to the three DL relevance tiers.

#### Scenario: Strong tier template (full article)
- **WHEN** generating content for a "strong" tier knowledge point
- **THEN** the article SHALL contain exactly these four sections:
  - `## 一、与[知识点名]的关联` — 从知识点的核心概念出发，自然引出在 DL 中的角色（~200-300 tokens）
  - `## 二、在深度学习中的具体应用` — 2-3 个具体场景，注明框架/库/论文名称（~300-500 tokens）
  - `## 三、代码/示例` — 简洁的代码片段（C/C++/Python 伪代码均可），展示如何在 DL 框架中使用该概念（~200-400 tokens）
  - `## 四、延伸阅读` — 2-3 个相关 `learn-deep` 文章的链接（格式：`- [文章标题](其他文章ID)`）（~50-100 tokens）
- **AND** the total article SHALL be ~1000-1500 tokens

#### Scenario: Medium tier template (concise article)
- **WHEN** generating content for a "medium" tier knowledge point
- **THEN** the article SHALL contain exactly these two sections:
  - `## 一、与[知识点名]的关联` — 从知识点核心概念出发，解释在 DL 中的连接点（~200-300 tokens）
  - `## 二、在深度学习中的体现` — 1-2 个具体例子（~200-300 tokens）
- **AND** code examples are optional, not required
- **AND** the total article SHALL be ~400-600 tokens

#### Scenario: Weak tier template (bridge paragraph)
- **WHEN** generating content for a "weak" tier knowledge point
- **THEN** the article SHALL be a single `## 深度学习中的关联` section with one paragraph (~100-200 tokens)
- **AND** the paragraph SHALL make at least one specific connection to a DL concept or tool, avoiding generic statements like "this is fundamental to all programming"

### Requirement: Existing article supplement format
When a knowledge point already has a learn-deep article, the system SHALL generate a supplementary DL chapter to append at the end, rather than creating a new file.

#### Scenario: Strong/medium existing article supplement
- **WHEN** a knowledge point has an existing article AND is classified as "strong" or "medium"
- **THEN** the supplementary content SHALL be a self-contained chapter titled `## 深度学习中的[知识点名]`
- **AND** the chapter SHALL follow the strong/medium template structure (minus the title-level "一、")
- **AND** the content SHALL integrate naturally with the existing article, not repeat its concepts

#### Scenario: Weak existing article supplement
- **WHEN** a knowledge point has an existing article AND is classified as "weak"
- **THEN** the supplementary content SHALL be a single paragraph appended under `## 深度学习中的关联`

#### Scenario: Reference linking
- **WHEN** any supplementary content mentions another learn-deep article
- **THEN** it SHALL use the format `- [文章标题](知识点的 item ID)` for cross-reference
- **AND** the referenced article SHALL exist in the ITEMS_REGISTRY

### Requirement: Batch generation orchestration
The content generation SHALL be organized into batches of ~15 items each, processed by individual LLM sub-agents in a Workflow.

#### Scenario: Batch composition
- **WHEN** the system partitions 300 items into batches
- **THEN** each batch SHALL contain items from the same technical stack (C/CPP/DS/PY/DB/VDB/Linux) to maintain thematic coherence
- **AND** items of different DL tiers MAY be mixed within a batch
- **AND** each batch SHALL contain exactly the items needed for the sub-agent to understand context (prefer ~15 items/batch, max 20)

#### Scenario: Batch prompt structure
- **WHEN** a sub-agent receives a batch for generation
- **THEN** its prompt SHALL include:
  - 1. The full ITEMS_REGISTRY definitions for all items in the batch
  - 2. The tier classification for each item
  - 3. A template example (one strong article) to establish tone and depth
  - 4. The explicit instruction to generate one markdown block per item, separated by `---`
  - 5. An instruction that output quality trumps speed — articles SHALL be specific, technically accurate, and avoid generic filler

#### Scenario: Output validation
- **WHEN** a batch generation completes
- **THEN** the output SHALL contain exactly one markdown section per item in the batch
- **AND** each section SHALL start with `<!-- item: <itemId> -->` for automated parsing
