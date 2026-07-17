/**
 * 面试题编译脚本 — MD → JS
 *
 * 用法：
 *   node scripts/build-interview-questions.js
 *
 * 扫描 data/interview-questions/{stack}/*.md
 * 输出 data/interview/interview-questions.js
 */

const fs = require("fs");
const path = require("path");

const ROOT = path.join(__dirname, "..");
const SRC_DIR = path.join(ROOT, "data", "interview-questions");
const OUT_FILE = path.join(ROOT, "data", "interview", "interview-questions.js");

const STACK_LABELS = {
  "c-language": { label: "C 语言", icon: "🔧", category: "lang" },
  "cpp-language": { label: "C++", icon: "⚙️", category: "lang" },
  redis: { label: "Redis", icon: "🗄️", category: "db" },
  mysql: { label: "MySQL", icon: "🗃️", category: "db" },
  os: { label: "操作系统", icon: "💻", category: "cs" },
  network: { label: "计算机网络", icon: "🌐", category: "cs" },
  "design-pattern": { label: "设计模式", icon: "🏗️", category: "cs" },
  python: { label: "Python", icon: "🐍", category: "lang" },
  ai: { label: "AI 相关", icon: "🤖", category: "ai" },
  "claude-code": { label: "Claude Code", icon: "📎", category: "other" },
};

function parseFrontmatter(content) {
  const lines = content.split("\n");
  if (lines.length < 2 || lines[0].trim() !== "---") {
    return { frontmatter: {}, body: content };
  }

  let endIdx = -1;
  for (let i = 1; i < lines.length; i++) {
    if (lines[i].trim() === "---") {
      endIdx = i;
      break;
    }
  }
  if (endIdx === -1) {
    return { frontmatter: {}, body: content };
  }

  const fmLines = lines.slice(1, endIdx);
  const body = lines
    .slice(endIdx + 1)
    .join("\n")
    .trim();
  const frontmatter = {};

  for (const line of fmLines) {
    const colonIdx = line.indexOf(":");
    if (colonIdx === -1) continue;
    const key = line.slice(0, colonIdx).trim();
    let val = line.slice(colonIdx + 1).trim();

    if (val.startsWith("[") && val.endsWith("]")) {
      val = val
        .slice(1, -1)
        .split(",")
        .map((s) =>
          s
            .trim()
            .replace(/^"(.*)"$/, "$1")
            .replace(/^'(.*)'$/, "$1"),
        );
    } else if (val === "true") val = true;
    else if (val === "false") val = false;
    else if (!isNaN(val)) val = Number(val);
    else val = val.replace(/^"(.*)"$/, "$1").replace(/^'(.*)'$/, "$1");

    // 支持嵌套键如 "tags: [a, b, c]"
    frontmatter[key] = val;
  }

  return { frontmatter, body };
}

function extractTitle(body) {
  const match = body.match(/^##\s+(.+)$/m);
  if (match) {
    // Remove the matched "## title" from body and return separately
    // But we keep body intact, just extract the title
    return match[1].trim();
  }
  return "（未命名题目）";
}

function scanMDFiles(dir, stackName) {
  const results = [];
  if (!fs.existsSync(dir)) return results;
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  for (const entry of entries) {
    if (entry.isFile() && entry.name.endsWith(".md")) {
      const filePath = path.join(dir, entry.name);
      const content = fs.readFileSync(filePath, "utf8");
      const { frontmatter, body } = parseFrontmatter(content);
      const id = entry.name.replace(/\.md$/, "");
      const fmId = frontmatter.id || id;

      if (fmId !== id) {
        console.warn(
          `  ⚠ 文件名 "${entry.name}" 与 frontmatter id "${fmId}" 不一致，使用 frontmatter id`,
        );
      }

      const title = frontmatter.title || extractTitle(body);
      const difficulty = frontmatter.difficulty || "medium";
      const priority = frontmatter.priority || "low";
      const tags = frontmatter.tags || [];

      if (!frontmatter.difficulty) {
        console.warn(`  ⚠ "${id}" 缺少 difficulty，使用默认值 "medium"`);
      }
      if (!frontmatter.priority) {
        console.warn(`  ⚠ "${id}" 缺少 priority，使用默认值 "low"`);
      }

      results.push({
        id: fmId,
        stack: stackName,
        chapter: stackName,
        difficulty,
        priority,
        tags,
        title,
        body: body,
      });
    }
  }
  return results;
}

function build() {
  console.log("📦 编译面试题数据...\n");

  if (!fs.existsSync(SRC_DIR)) {
    console.log(`  ${SRC_DIR} 不存在，创建空目录`);
    fs.mkdirSync(SRC_DIR, { recursive: true });
  }

  const stacks = fs
    .readdirSync(SRC_DIR, { withFileTypes: true })
    .filter((d) => d.isDirectory())
    .map((d) => d.name);

  const allQuestions = [];
  const stackSet = new Set();

  for (const stack of stacks) {
    const dir = path.join(SRC_DIR, stack);
    const questions = scanMDFiles(dir, stack);
    allQuestions.push(...questions);
    if (questions.length > 0) {
      stackSet.add(stack);
      console.log(`  ${stack} (${questions.length} 题)`);
    }
  }

  const stackEntries = Object.entries(STACK_LABELS)
    .filter(([id]) => stackSet.has(id))
    .map(([id, info]) => ({ id, ...info }));

  // 如果有的栈在 STACK_LABELS 中没定义但目录存在，也加进去
  for (const stack of stacks) {
    if (!STACK_LABELS[stack] && stackSet.has(stack)) {
      stackEntries.push({ id: stack, label: stack, icon: "📄" });
    }
  }

  // 构建分类列表
  const categoryMap = {};
  for (const s of stackEntries) {
    if (!s.category) continue;
    if (!categoryMap[s.category]) {
      categoryMap[s.category] = {
        id: s.category,
        label: { lang: "语言", db: "数据库", cs: "计算机", ai: "AI", other: "其他" }[s.category] || s.category,
        icon: { lang: "💬", db: "🗄️", cs: "💻", ai: "🤖", other: "📋" }[s.category] || "",
      };
    }
  }
  const interviewCategories = Object.values(categoryMap);

  if (allQuestions.length === 0) {
    console.log("\n⚠ 未找到任何 MD 文件，跳过写入（保留现有数据文件）");
    return;
  }

  const output = `/* ============================================================
   面试题库数据 — 由 scripts/build-interview-questions.js 自动生成
   生成时间: ${new Date().toISOString()}
   源文件: data/interview-questions/
   ============================================================ */

const INTERVIEW_CATEGORIES = ${JSON.stringify(interviewCategories, null, 2)};

const INTERVIEW_STACKS = ${JSON.stringify(stackEntries, null, 2)};

const INTERVIEW_QUESTIONS = ${JSON.stringify(allQuestions, null, 2)};
`;

  fs.writeFileSync(OUT_FILE, output, "utf8");
  console.log(`\n✅ 输出: ${OUT_FILE} (${allQuestions.length} 题)`);
}

build();
