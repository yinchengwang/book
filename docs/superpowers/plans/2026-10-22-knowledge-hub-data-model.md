# Knowledge Hub → 原生微信小程序 数据模型与页面交互清单

> 本文件通过**逐行阅读** `engineering/apps/web/knowledge_hub/src/` 下的 14 个 store、16 个页面目录、相关 components / services / utils 生成。
> 它是改写（Taro 3.6 + React 18 + Zustand → 纯 JS 原生小程序）的**唯一事实来源（single source of truth）**。
> 所有算法均逐字引用源码，可直接移植到 `core/`。

---

## 总览映射表（14 个 store → 14 个 `db:` key）

| # | Target key | 源文件 | Zustand 形态 | 适配 `core/store.js` 的方式 |
|---|-----------|--------|--------------|------------------------------|
| 1 | `db:daily` | `stores/dailyDataStore.ts` | **对象型（复杂）**：`snapshots:[]` + `dailyLogs:{}` + 一堆派生方法 | `createObjectStore` 勉强可存整体，但方法需保留为自定义逻辑 |
| 2 | `db:excerpts` | `stores/excerptStore.ts` | **列表型**：`excerpts[]` | `createListStore({ key:'db:excerpts', idField:'id' })` |
| 3 | `db:five_year_plan` | `stores/fiveYearPlanStore.ts` | **对象型（嵌套树）**：`data: Record<year, Record<month, Record<day, DayData>>>` | 自定义对象 store（树结构，不是扁平对象） |
| 4 | `db:interview_bank` | `stores/interviewBankStore.ts` | **对象型**：`viewed:[]` + `bookmarks:[]` | `createObjectStore({ key:'db:interview_bank' })` |
| 5 | `db:interviews` | `stores/interviewStore.ts` | **列表型**：`companies[]` | `createListStore({ key:'db:interviews', idField:'id' })` |
| 6 | `db:knowledge` | `stores/knowledgeStore.ts` | **列表型**：`nodes[]`（外加 `links`/`knowledgeGraph`/`gaps`） | `createListStore({ key:'db:knowledge', idField:'id' })`，links 作为节点附带字段或独立 list |
| 7 | `db:learning_paths` | `stores/learningPathStore.ts` | **列表型**：`paths[]`（外加 `completedSubSteps` 映射） | `createListStore({ key:'db:learning_paths', idField:'id' })` |
| 8 | `db:mock_interviews` | `stores/mockInterviewStore.ts` | **列表型**：`sessionHistory[]` | `createListStore({ key:'db:mock_interviews', idField:'id' })` |
| 9 | `db:notifications` | `stores/notificationStore.ts` | **对象型**：`items[]` + `unreadCount` | 自定义对象 store；多半改为「派生」而非持久化 |
| 10 | `db:study_projects` | `stores/projectStore.ts`（重命名 project→study_projects） | **列表型**：`projects[]` | `createListStore({ key:'db:study_projects', idField:'id' })` |
| 11 | `db:quizzes` | `stores/quizStore.ts` | **复杂**：`questions[]` + `answers[]` + `dailyPlans[]` + `stackProgress{}` | 拆分为 `db:quizzes`（answers）等，或自定义 store |
| 12 | `db:reviews` | `stores/reviewStore.ts` | **列表型**：`records[]`（外加 `dueReviews`/`currentReview` 派生） | `createListStore({ key:'db:reviews', idField:'id' })` |
| 13 | `db:settings` | `stores/settingsStore.ts` | **对象型** | `createObjectStore({ key:'db:settings', defaultValue:{...} })` |
| 14 | `db:user` | `stores/userStore.ts` | **对象型**：`user` | `createObjectStore({ key:'db:user', defaultValue:{...} })` |

> ⚠️ 见 Part E 风险 #2：工厂只覆盖「简单列表 / 简单对象」。本表凡标注「复杂 / 嵌套 / 派生」的 store 不能完全套用工厂，需要自定义 store 或扩展工厂。

---

# Part A — 逐 Store Schema

## 1. `db:daily` ← `dailyDataStore.ts`
**形态**：对象型（复杂）。Zustand persist name = `daily-data-storage`（改写需改为 `db:daily`）。

**Record / 整体对象结构**
```ts
interface Snapshot {
  date: string            // 'YYYY-MM-DD'
  scores: Record<string, number>   // domain -> score
  totalScore: number      // 综合掌握度
}
interface DailyLog {
  date: string
  studied: string[]       // 题目 ID 列表
  correct: string[]
  wrong: string[]
  reviewCount: number     // 复习次数
  studyMinutes: number    // 学习时长（分钟）
}
// 持久化整体 = { snapshots: Snapshot[], dailyLogs: Record<date, DailyLog> }
```

**Operations**
- `addSnapshot(snapshot)` — 按 `snapshot.date` upsert 到 `snapshots`。
- `getRecentSnapshots(days)` — 返回最近 N 天（按 `date` 字符串比较）升序。
- `getDailyLog(date)` — 取 `dailyLogs[date]`。
- `addDailyLog(log)` — `dailyLogs[date] = log`（覆盖写）。
- `recordStudy(itemId, correct)` — 以「今天」ISO 日期为 key，把 itemId 加入 `studied`，并按 correct 加入 `correct`/`wrong`（用 `Set` 去重）。
- `recordReview(count=1)` — 今天 `reviewCount += count`。
- `recordStudyTime(minutes)` — 今天 `studyMinutes += minutes`。
- `getHeatmapData()` — 取「最近一年」每天强度 = `studied.length + reviewCount`。

**Algorithms / derived**
```ts
// createDailySnapshot (导出函数，非 store action)
export const createDailySnapshot = (scores: Record<string, number>) => {
  const today = new Date().toISOString().slice(0, 10)
  const totalScore = Object.values(scores).reduce((a, b) => a + b, 0) /
                     Math.max(Object.values(scores).length, 1)
  useDailyDataStore.getState().addSnapshot({ date: today, scores, totalScore })
}
```

**Seed / initial data**：`snapshots: []`，`dailyLogs: {}`（空）。

---

## 2. `db:excerpts` ← `excerptStore.ts`
**形态**：列表型（数组 `excerpts`）。persist name = `excerpt-storage`，`version:2`。

**数组元素 Excerpt**
```ts
interface Excerpt {
  id: string
  content: string
  book: string
  tags: string[]            // ⚠️ 老版本是 string，migrate v1→v2 强制数组化
  note?: string
  favorite: boolean
  createdAt: string
  date?: string             // 年份（2024 / 2025）
  source?: string           // 源文件名（不含 .md 后缀）
  status?: 'reading' | 'read' | 'mastered' | 'wishlist'
}
```
**Sample（迁移数据形态，来自 `@/data/excerpt/initialExcerpts`，该文件不在快照中—见风险#1）**：
```ts
{ id:'excerpt-1', content:'...', book:'代码大全', tags:['工程'], note:'',
  favorite:false, createdAt:'2024-...', date:'2024', source:'xxx', status:'read' }
```

**Operations**
- `addExcerpt(excerpt)` — 头部 unshift。
- `deleteExcerpt(id)` — 过滤删除。
- `toggleFavorite(id)` — 翻转 `favorite`。
- `updateNote(id, note)` — 更新 `note`。
- `setStatus(id, status)` — 更新 `status`。
- `setFilterBook / setFilterTag / setFilterYear / setSearchQuery` — **UI 态，不属于领域数据**（改写应剥离到页面局部 state）。

**Derived hook** `useExcerptDerivedData()`：`books = unique(book)`，`tags = unique(flatMap tags)`，`years = unique(date)`，`stats = { total, favorites, reading, mastered, books }`。

**Seed**：`excerpts: initialExcerpts`（= 4 条演示 + 老项目 145 条迁移；`initialExcerpts` 来自缺失文件）。`migrate`：`version<2` 时把老 tags 规范为数组、按 id 去重合并。

---

## 3. `db:five_year_plan` ← `fiveYearPlanStore.ts`
**形态**：对象型（嵌套树）。persist name = `rr2-five-year-plan`。

**核心结构**
```ts
interface DayData {
  checks: Record<string, boolean>   // habitId -> checked
  note: string
  hasLearning?: boolean
}
// data: Record<year, Record<month('06'), Record<day('08'), DayData>>>
```
习惯项（habitId）来自 `@/data/five_year_plan_config` 的 `getAllItems(year)`（文件缺失—风险#1）。

**Operations**
- `getDay(year, month, day)` / `setDay(...)` / `deleteDay(...)`。
- `toggleCheck(year, month, day, habitId)`。
- `toggleLearning(year, month, day)`。
- `countDayChecks(year, month, day)` → `{ done, total }`（total 来自 `getAllItems`）。
- `getStreak()` — **连续打卡天数算法**：
```ts
getStreak: () => {
  const d = new Date()
  let streak = 0
  for (let i = 0; i < 365; i++) {
    const { done } = get().countDayChecks(d.getFullYear(), d.getMonth() + 1, d.getDate())
    if (done > 0) { streak++; d.setDate(d.getDate() - 1) } else break
  }
  return streak
}
```
- `getTotalDays()` — 遍历树，统计任一 `checks` 为 true 的天数。
- `getMonthPct(year, month)` — 当月 `done/total` 百分比（按天聚合）。

**Seed**：`data: {}`（空）。

---

## 4. `db:interview_bank` ← `interviewBankStore.ts`
**形态**：对象型。persist name = `rr2-interview-bank`。

```ts
interface InterviewBankState {
  viewed: string[]     // 已浏览题目 ID
  bookmarks: string[]  // 收藏题目 ID
}
```
**Operations**：`markViewed(id)`（去重追加）、`toggleBookmark(id)`、`isViewed(id)`、`isBookmarked(id)`、`clearViewed()`。
**Seed**：`viewed: []`，`bookmarks: []`。

---

## 5. `db:interviews` ← `interviewStore.ts`
**形态**：列表型（`companies`）。persist name = `interview-storage`。

**数组元素 Company**
```ts
interface InterviewRecord { id: string; date: string; content: string;
  type: '一面'|'二面'|'三面'|'HR'|'笔试' }
interface Company {
  id: string
  name: string
  position: string
  status: '投递'|'笔试'|'一面'|'二面'|'三面'|'HR'|'offer'|'拒绝'
  salary?: string
  notes?: string
  timeline: InterviewRecord[]
  createdAt: string
}
```
**Sample**：`company-1` 字节跳动 / 后端 / 二面 / 35-50K / 3 条 timeline；`company-2` 阿里巴巴。

**Operations**：`addCompany`、`removeCompany`、`updateStatus(id, status)`、`addInterviewRecord(companyId, record)`（自动生成 `rec-${Date.now()}` 作为 id）。

**⚠️ 冗余派生字段**：`totalInterviews`、`upcomingInterviews`（= `status` 非 offer/拒绝）在 action 内被**重复计算并写入 state**。改写时应改为纯派生（getter），不要持久化。

**Seed**：2 家（见上）。

---

## 6. `db:knowledge` ← `knowledgeStore.ts`
**形态**：列表型（`nodes`），persist name = `knowledge-storage`，`version:2`。

```ts
interface KnowledgeNode { id: string; name: string; domain: string; level: number; parentId?: string }
interface KnowledgeLink { source: string; target: string; type: string }
interface DomainLevel { level: number; count: number }
// state 同时持有 nodes[], links[], knowledgeGraph{nodes,links}, gaps{algorithm?,system?,cpp?,database?,network?}, totalKnowledge
```
**Seed nodes（14 个）**：`algo-1..4`（算法）、`sys-1..3`（系统设计）、`cpp-1..3`（C/C++）、`db-1..2`（数据库）、`net-1..2`（计算机网络），level 1~4。**Seed links（9 条）**：全为 `type:'prerequisite'` 的链式前置边。

**Operations**：`addNode`、`removeNode`（同时清理 links 与 knowledgeGraph）、`updateNodeLevel(id, level)`、`loadGaps()`。

**Algorithm `loadGaps()`（逐字移植）**：
```ts
loadGaps: () => {
  const nodes = get().nodes
  const domains = ['算法', '系统设计', 'C/C++', '数据库', '计算机网络']
  const gaps: Record<string, DomainLevel> = {}
  domains.forEach(domain => {
    const domainNodes = nodes.filter(n => n.domain === domain)
    const avgLevel = domainNodes.length > 0
      ? Math.round(domainNodes.reduce((acc, n) => acc + n.level, 0) / domainNodes.length * 20)
      : 0
    gaps[domain] = { level: avgLevel, count: domainNodes.length }
  })
  set({ gaps: {
    algorithm: gaps['算法'], system: gaps['系统设计'], cpp: gaps['C/C++'],
    database: gaps['数据库'], network: gaps['计算机网络'] } })
}
```
**Seed**：`gaps: {}`（v2 migrate 清空），`totalKnowledge: 14`。

**⚠️ 冗余**：`knowledgeGraph` 与 `nodes`/`links` 重复；改写时 `links` 可并入节点或独立 list。

---

## 7. `db:learning_paths` ← `learningPathStore.ts`
**形态**：列表型（`paths`），persist name = `learning-path-storage`，`version:2`。

```ts
type SubStepType = 'knowledge' | 'practice' | 'project'
interface SubStep { id: string; title: string; type: SubStepType;
  estimatedHours: number; refId?: string; done?: boolean }
interface LearningStep { id: string; title: string; description: string;
  difficulty: '入门'|'初级'|'中级'|'高级'; estimatedHours: number;
  resources: string[]; subSteps: SubStep[] }
interface LearningPath { id: string; name: string; description: string; icon: string;
  difficulty: '入门'|'初级'|'中级'|'高级'|'专家'; estimatedHours: number;
  steps: LearningStep[]; currentStep: number }
```
**Seed**：3 条路径（`cpp-master` 6 步、`algorithm` 6 步、`system-design` 4 步），每步含 4~7 个 subSteps（数据量较大，需整段移植）。

**Operations**：`setActivePath(id)`、`nextStep`、`prevStep`、`resetPath`、`toggleSubStep(stepId, subStepId)`（key=`${stepId}:${subStepId}`）、`getStepProgress(stepId)`、`getPathProgress(pathId)`。

**Algorithm 进度**：`percent = round(done/total*100)`，done = `completedSubSteps` 命中数。

**⚠️ 持久化边界**：`activePathId`/`activePath`/`currentStep` 为 UI/派生态，不应进 `db:`；真正需持久化的仅是 `completedSubSteps`。`version<2` migrate 用 `initialPaths` 整体替换并清空 active（避免老 step 缺 subSteps 崩溃）。

---

## 8. `db:mock_interviews` ← `mockInterviewStore.ts`
**形态**：列表型（`sessionHistory`）。persist name = `mock-interview-storage`。

```ts
interface InterviewQuestion { question: string; answer: string;
  evaluation?: { score: number; feedback: string; suggestions: string[] } }
interface MockSession { id: string; date: string; role: string; difficulty: string;
  questions: InterviewQuestion[]; totalScore: number; duration?: number }
```
**Seed**：1 个 `session-1`（昨天，后端开发/中级，1 题，score 85）。

**Operations**：`addSession(session)`（同时设 `currentSession`）、`deleteSession(id)`、`clearHistory()`、`getSessionById(id)`。

---

## 9. `db:notifications` ← `notificationStore.ts`
**形态**：对象型（`items[]` + `unreadCount`）。**无 persist**（纯内存）。

```ts
interface Notification { id: string; type:'review'|'interview'|'streak'|'wrongbook'|'system';
  title: string; desc?: string; time: string; timestamp: number; link?: string;
  icon: string; iconBg: string; read: boolean }
```
**Operations**：`setItems(items)`（重算 unreadCount）、`markRead(id)`、`markAllRead()`。
**Algorithm**：
```ts
export function timeAgo(timestamp: number): string {
  const diff = Date.now() - timestamp
  const minutes = Math.floor(diff / 60000)
  if (minutes < 1) return '刚刚'
  if (minutes < 60) return `${minutes} 分钟前`
  const hours = Math.floor(minutes / 60)
  if (hours < 24) return `${hours} 小时前`
  const days = Math.floor(hours / 24)
  if (days === 1) return '昨天'
  if (days < 7) return `${days} 天前`
  return new Date(timestamp).toLocaleDateString('zh-CN')
}
```
**⚠️ 实际上没有任何页面直接使用 notificationStore**（无 notification 页面）；它应为「派生」数据（由 review/interview/fiveYearPlan/quiz 推导），改写时建议不持久化、按需生成。

---

## 10. `db:study_projects` ← `projectStore.ts`（注意重命名）
**形态**：列表型（`projects`）。persist name = `project-storage`。

```ts
interface Project { id: string; name: string; description?: string;
  status: '规划中'|'进行中'|'已完成'|'暂停'|'废弃';
  priority: '高'|'中'|'低'; tags: string[]; knowledgeLinks: string[];
  createdAt: string; updatedAt: string }
```
**Sample**：`project-1` 手写 STL 容器库 / 进行中 / 高；`project-2` 分布式缓存系统 / 规划中 / 中；`project-3` LeetCode 刷题笔记 / 已完成 / 中。

**Operations**：`addProject`（unshift）、`updateProject(id, updates)`（自动 `updatedAt = new Date().toISOString()`）、`deleteProject`。

**Seed**：3 个（见上）。

---

## 11. `db:quizzes` ← `quizStore.ts`
**形态**：**复杂**，persist name = `quiz-storage`，`version:2`。核心 `questions[]` 来自缺失的 `@/data/quiz-index`。

```ts
interface QuizQuestion { id: string; title: string; description?: string;
  category: '选择题'|'简答题'|'编程题'|'判断题'|'填空题';
  difficulty: '入门'|'初级'|'中级'|'高级'|'专家'; domain: string;
  options?: string[]; answer: string; explanation?: string;
  timeEstimate: number; tags?: string[] }
interface QuizAnswer { questionId: string; userAnswer: string; correct: boolean;
  date: 'YYYY-MM-DD'; timestamp: number; timeSpent?: number }
interface DailyPlan { date: 'YYYY-MM-DD'; questionIds: string[]; stack: string; completed: number }
interface StackProgress { stack: string; total: number; answered: number;
  correct: number; lastUpdated: string }
```

**Operations**：`startSession(questionIds)`、`submitAnswer(answer, timeSpent?)`、`nextQuestion`、`prevQuestion`、`endSession`、`resetSession`、`generateDailyPlan(date?)`、`getTodayPlan()`、`updatePlanProgress(questionId)`、`getWrongBook()`、`isWrongAnswer(qid)`、`getProgress(stack?)`、`getStats()`、`getQuestionById(id)`、`searchQuestions(query)`。

**⚠️ `currentSession` 是纯 UI 态**，不应持久化。

**Algorithms（逐字）**
```ts
// 连续学习天数
function calculateStreak(answers: QuizAnswer[]): number {
  if (answers.length === 0) return 0
  const dates = new Set(answers.map(a => a.date))
  const sorted = Array.from(dates).sort().reverse()
  let streak = 0
  const today = new Date()
  for (let i = 0; i < sorted.length; i++) {
    const d = new Date(sorted[i])
    const diff = Math.floor((today.getTime() - d.getTime()) / 86400000)
    if (diff === streak) streak++
    else break
  }
  return streak
}
```
```ts
// submitAnswer 判分逻辑
const userAns = String(answer).trim()
const correctAns = String(q.answer).trim()
const correct: boolean = userAns === correctAns ||
  (Boolean(q.options) && userAns === correctAns.charAt(0))
```
```ts
// 每日刷题计划：按 stack 轮换，每天推 10 道（4 简单 + 4 中等 + 2 困难）
const STACK_ORDER = ['c', 'cpp', 'db', 'ds', 'linux', 'py', 'vdb']
generateDailyPlan: (date) => {
  const daysSinceEpoch = Math.floor(Date.now() / 86400000)
  const stackIndex = daysSinceEpoch % STACK_ORDER.length
  const stack = STACK_ORDER[stackIndex]
  // 优先选用户没答过的；不足 5 道则用原选择
  ...
}
```
```ts
// 错题本：同题仅保留最近一次错答
getWrongBook: () => {
  const wrongMap = new Map<string, QuizAnswer>()
  for (const a of get().answers) {
    if (!a.correct) {
      const existing = wrongMap.get(a.questionId)
      if (!existing || a.timestamp > existing.timestamp) wrongMap.set(a.questionId, a)
    }
  }
  return Array.from(wrongMap.values()).sort((a, b) => b.timestamp - a.timestamp)
}
```
```ts
getStats: () => {
  const answers = get().answers
  const totalAnswered = answers.length
  const totalCorrect = answers.filter(a => a.correct).length
  const accuracy = totalAnswered > 0 ? totalCorrect / totalAnswered : 0
  const streak = calculateStreak(answers)
  return { totalAnswered, totalCorrect, streak, accuracy }
}
```
**Seed**：`questions = allQuizQuestions`（缺失）、`answers: []`、`dailyPlans: []`、`stackProgress: {}`。

---

## 12. `db:reviews` ← `reviewStore.ts`（SM-2）
**形态**：列表型（`records`），persist name = `review-storage`。详见 **Part B**。

```ts
interface ReviewRecord { id: string; question: string; answer: string; domain: string;
  easiness: number; interval: number; repetitions: number;
  nextReviewDate: string; lastReviewDate: string }
```
**Seed**：3 条（`review-1` 时间复杂度/算法、`review-2` 智能指针/C-C++、`review-3` TCP-UDP/计算机网络），`easiness:2.5, interval:1, repetitions:0`。

**Operations**：`loadDueReviews`、`rateReview(quality)`、`nextReview`、`calculateNextReview(record, quality)`、`addRecord(record)`（缺省字段 `easiness:2.5, interval:1, repetitions:0, nextReviewDate:now, lastReviewDate:''`）。
**⚠️ `dueReviews` / `currentReview` / `currentIndex` / `loading` 为派生/UI 态，不持久化。**

---

## 13. `db:settings` ← `settingsStore.ts`
**形态**：对象型。persist name = `settings-storage`。

```ts
interface ApiConfig { apiKey: string; apiUrl: string; model: string }
interface NotificationConfig { reviewReminder: boolean; interviewReminder: boolean }
// state: { theme:'light'|'dark', apiConfig, notifications,
//          setTheme, setApiConfig, setNotifications, exportData, importData, clearData }
```
**Seed**：`theme:'dark'`、`apiConfig:{ apiKey:'', apiUrl:'https://api.openai.com/v1', model:'gpt-4' }`、`notifications:{ reviewReminder:true, interviewReminder:true }`。

**⚠️ 浏览器专用**：`exportData`/`importData`/`clearData` 内部使用 `localStorage`、`Blob`、`URL.createObjectURL`、`document.createElement('a')`、`window.location.reload()`。原生小程序不可用，须重写为 `wx.setClipboardData` / `wx.getFileSystemManager` / `wx.removeStorageSync`。

---

## 14. `db:user` ← `userStore.ts`
**形态**：对象型（`user`）。persist name = `rr2-user`。

```ts
interface UserInfo { name: string; avatar: string; email?: string;
  joinDate: string; bio?: string }
```
**Helper**：
```ts
export function getInitial(name: string): string {
  if (!name || !name.trim()) return '?'
  const trimmed = name.trim()
  if (/^[a-zA-Z\s]/.test(trimmed)) return trimmed.charAt(0).toUpperCase()
  return trimmed.charAt(0)
}
```
**Operations**：`setName`（同步 `avatar` 首字母，除非用户手动设过）、`setEmail`、`setBio`、`setAvatar`、`reset`（保留 joinDate）。
**Seed**：`{ name:'Yinch', avatar:'Y', joinDate: now, bio:'Reading Radar 学习者' }`。

---

# Part B — reviewStore SM-2（逐字确认，可直移植到 `core/review.js`）

```ts
// SM-2 算法核心：计算下次复习时间
calculateNextReview: (record, quality) => {
  let { easiness, interval, repetitions } = record

  // 更新难度因子
  easiness = Math.max(1.3, easiness + (0.1 - (5 - quality) * (0.08 + (5 - quality) * 0.02)))

  if (quality < 3) {
    // 质量低于 3，重置间隔
    repetitions = 0
    interval = 1
  } else {
    // 质量 >= 3，增加间隔
    if (repetitions === 0) {
      interval = 1
    } else if (repetitions === 1) {
      interval = 6
    } else {
      interval = Math.round(interval * easiness)
    }
    repetitions += 1
  }

  // 计算下次复习日期
  const nextDate = new Date()
  nextDate.setDate(nextDate.getDate() + interval)

  return {
    ...record,
    easiness,
    interval,
    repetitions,
    nextReviewDate: nextDate.toISOString(),
    lastReviewDate: new Date().toISOString()
  }
}
```

```ts
// 对复习质量评分
rateReview: (quality) => {
  const { currentReview, dueReviews, currentIndex, records } = get()
  if (!currentReview) return

  // 使用 SM-2 算法计算下次复习时间
  const updatedRecord = get().calculateNextReview(currentReview, quality)

  // 更新 records
  const updatedRecords = records.map(r =>
    r.id === updatedRecord.id ? updatedRecord : r
  )

  // 更新待复习列表（过滤掉已到期的）
  const updatedDue = dueReviews.map(r =>
    r.id === updatedRecord.id ? updatedRecord : r
  ).filter(r => new Date(r.nextReviewDate) <= new Date())

  // 移动到下一个
  const nextIndex = currentIndex + 1
  const nextReview = nextIndex < updatedDue.length ? updatedDue[nextIndex] : null

  set({
    records: updatedRecords,
    dueReviews: updatedDue,
    currentReview: nextReview,
    currentIndex: nextIndex
  })
}
```

```ts
// 添加新的复习记录（缺省字段）
addRecord: (record) => {
  const newRecord: ReviewRecord = {
    ...record,
    easiness: 2.5,
    interval: 1,
    repetitions: 0,
    nextReviewDate: new Date().toISOString(),
    lastReviewDate: ''
  }
  set(state => ({
    records: [...state.records, newRecord],
    dueReviews: [...state.dueReviews, newRecord]
  }))
}
```

```ts
// 加载待复习记录
loadDueReviews: () => {
  set({ loading: true })
  const now = new Date()
  const due = get().records.filter(record => new Date(record.nextReviewDate) <= now)
  set({ dueReviews: due, currentReview: due.length > 0 ? due[0] : null, currentIndex: 0, loading: false })
}
```

---

# Part C — 逐页面交互地图

> 路由来自 `app.config.ts` 与 `utils/routes.ts`。`index.tsx` 是 Taro 入口（仅 `render children`），非页面。共 16 个页面目录，其中 11 个在 `app.config` 注册，5 个经 `navigateTo` 进入（quiz / radar / interview_bank / five_year_plan / learn_deep）。

## C-1 `pages/index`（Dashboard，TabBar 首页）
- **Route**：`/pages/index`（tab）。
- **Stores**：quizStore、fiveYearPlanStore、interviewBankStore、dailyDataStore。
- **目的**：总览仪表盘（复刻 dashboard.html）。
- **Key interactions**：
  - 七年技术栈卡 `onClick → navigate('/quiz')`（H5 用 `window.history.pushState` + `popstate`，原生改 `wx.navigateTo`）。
  - 五年计划卡 `onClick → navigate('/five_year_plan')`。
  - 盲点项 `onClick → navigate('/quiz')`。
  - 最近测验记录 `link → navigate('/quiz')`。
  - 路线图 stage / 「查看完整计划」→ `/five_year_plan`。
- **Charts**：`HeatmapCalendar`（GitHub 风格热力图，纯 div 网格，`<canvas>` 可复用但非必须）、`TrendChart`（14 天趋势；H5 用 Recharts，**MP 用占位 div 柱状**，原生须 `<canvas type="2d">` 重写）。
- **Algorithms**：`stackStats`（每栈 tested/attempts/avgScore）、`blindSpots`（取最近 3 答，错率→得分，<65 标记，取前 8）、`recentRecords`（最近 10 条映射 stack）。
- **Markdown**：无。

## C-2 `pages/review/Review`（TabBar）
- **Route**：`/pages/review/Review`（tab）。
- **Stores**：reviewStore（SM-2）。
- **Key interactions**：
  - 6 个评分按钮 `0..5 → rateReview(q)`。
  - 顶部分类卡 / Tab `due|all|done → setTab`。
  - 「继续下一轮」`nextReview()`。
- **Derived**：`doneRecords`（repetitions>0 且 nextReviewDate>now）、`upcomingRecords`（7 天内）、`overdueRecords`（已过期且 repetitions=0）。
- **Charts/MD**：无。

## C-3 `pages/knowledge_graph/knowledge_graph`
- **Route**：`/pages/knowledge_graph/knowledge_graph`（navigateTo）。
- **Stores**：knowledgeStore、quizStore。
- **Key interactions**：
  - 节点点击 → `setSelectedId(id)` 弹 modal。
  - modal 内 `updateNodeLevel(id, level)`、`-/+` 调整 `adjustLevel(±1)`。
  - 关联节点点击切换选中。
- **Charts**：**原始 `<svg>` 树状布局**（按 domain 分列 + prerequisite 连边 + 箭头 marker + 半径 = `8 + level*2`）。坐标 JS 计算。原生**不支持 SVG DOM**，须改写 `<canvas type="2d">` 或 `canvas` 2d。
- **Algorithm**：`positions`（domain 沿 x、节点沿 y 均匀分布）、窗口 resize 监听（`window.innerWidth`，原生改 `wx.getSystemInfoSync`）。

## C-4 `pages/gap_analysis/gap_analysis`
- **Route**：`/pages/gap_analysis/gap_analysis`（navigateTo）。
- **Stores**：knowledgeStore（`loadGaps`、`gaps`）、quizStore。
- **Key interactions**：空状态 CTA `navigateTo('/quiz')`。
- **Algorithm**：`gapData` 目标（算法80/系统70/C++75/数据库65/网络60）vs `current`（来自 `gaps[key].level`），`gap = max(0, target-current)`，`isUrgent = gap>20` 给建议。无学习数据时显示空状态引导。

## C-5 `pages/interview_tracker/interview_tracker`（TabBar）
- **Route**：`/pages/interview_tracker/interview_tracker`（tab）。
- **Stores**：interviewStore。
- **Key interactions**：
  - 「+ 添加公司」→ `addCompany({ id:'company-'+Date.now(), ... })`。
  - 状态 chip → `handleStatusChange → updateStatus`。
  - 删除 → `confirm()` + `removeCompany`。
  - timeline 输入（日期:内容）→ `addInterviewRecord`。
  - 「分享」→ `shareCompany`（生成 markdown 面经，H5 用 `navigator.clipboard`/`Blob`，原生改 `wx.setClipboardData`）。
- **Static 颜色**：`statusColors` 8 状态、`statusProgress` 进度映射（见 Part D）。

## C-6 `pages/learning_path/learning_path`（TabBar）
- **Route**：`/pages/learning_path/learning_path`（tab）。
- **Stores**：learningPathStore。
- **Key interactions**：路径 tab → `setActivePath`；sub-step 勾选 → `toggleSubStep`；`nextStep/prevStep/resetPath`；重置确认 → `resetPath()`。
- **Algorithm**：`getStepProgress` / `getPathProgress`（done/total%）。`safeSteps = steps.filter(s => Array.isArray(s.subSteps))` 防老数据崩溃。
- **MD/Chart**：无。

## C-7 `pages/settings/Settings`（TabBar）
- **Route**：`/pages/settings/Settings`（tab）。
- **Stores**：settingsStore、userStore。
- **Key interactions**：
  - 深色模式 Switch → `setTheme`。
  - 用户资料保存 → `setName/setEmail/setBio`。
  - API 配置 → `setApiConfig({...})`。
  - 通知 → `setNotifications({...})`。
  - 导出/导入/清空 → `exportData/importData/clearData`（**浏览器专用，见风险**）。
  - 复制 API key → `navigator.clipboard`（原生改 `wx.setClipboardData`）。
- **MD/Chart**：无。

## C-8 `pages/excerpt/Excerpt`（navigateTo）
- **Route**：`/pages/excerpt/Excerpt`。
- **Stores**：excerptStore（+ `useExcerptDerivedData`）。
- **Key interactions**：
  - 搜索框 → `setSearchQuery`；书籍/标签/年份 chip → `setFilter*`。
  - 「+ 添加摘抄」→ `addExcerpt({ id:'excerpt-new-'+Date.now(), ... })`。
  - 卡片点击 → 详情 modal；收藏 → `toggleFavorite`；删除 → `deleteExcerpt`；`updateNote`；状态循环 `cycleStatus` → `setStatus`；复制 → `handleCopy`（navigator.clipboard / `wx.setClipboardData`）。
- **MD**：无（纯文本展示）。

## C-9 `pages/mock_interview/mock_interview`（navigateTo）
- **Route**：`/pages/mock_interview/mock_interview`。
- **Stores**：mockInterviewStore、settingsStore（`apiConfig`）。
- **Key interactions**：配置（岗位/难度/时长/题数/AI 开关）→ `config`；`startInterview`；答题 `answers[currentQ]`；`handleFinish` → `addSession({ id:'session-'+Date.now(), ... })`；`handleNext`/`handlePrev`。
- **Timer**：`setInterval` 倒计时（`useEffect`，依赖 `step==='interview'`），原生须 `onUnload` 清理。
- **Algorithm**：`mockAIService.evaluateAnswer`（规则评分：字数<20 -20、无结构 -10、>=90/75/60 分级）、`generateFollowUp`（按关键词 LRU 等匹配）、`generateNewQuestion`。**均为 mock，未接真实 AI**。
- **MD**：无（结果页硬编码文案）。
- **⚠️ 死代码**：结果页「导出面经」按钮无 `onClick` 处理。

## C-10 `pages/project_roadmap/project_roadmap`（navigateTo）
- **Route**：`/pages/project_roadmap/project_roadmap`。
- **Stores**：projectStore（→ `db:study_projects`）。
- **Key interactions**：添加 → `addProject({ id:'project-'+Date.now(), ... })`；详情 modal 内 `updateProject`（状态切换）、`deleteProject`。
- **Static 颜色**：`statusColors`（5 状态）、`priorityColors`（高/中/低）。

## C-11 `pages/quiz/Quiz`（navigateTo）
- **Route**：`/pages/quiz/Quiz`。
- **Stores**：quizStore（**核心页面**）。
- **5 种 mode**：`plan | list | quiz | wrong | stats`。
- **Key interactions**：
  - `startTodayPlan`（今日计划）→ `startSession(plan.questionIds)`；「换一批」→ `generateDailyPlan()`。
  - `startListQuiz`（前 20 题）；`startWrongQuiz`（前 10 错题）。
  - 答题 `handleSubmit` → `submitAnswer(answer, timeSpent)`；`handleNext`（末题 `endSession`）→ `mode='plan'`；`prevQuestion`。
  - 列表卡「直接答此题」→ `startSession([q.id])`。
- **Algorithms**：每日计划生成（`STACK_ORDER` 轮换 + 难度抽样）、判分（`userAns===answer || options && userAns===answer.charAt(0)`）、`getStats`（accuracy + streak）、`getWrongBook`。
- **MD**：题目 `explanation` 以纯文本展示（非 markdown）。
- **⚠️ 大列表**：题库列表 `.slice(0,50)`、错题 `.slice(0,10)`，原生须注意长列表性能（虚拟列表）。

## C-12 `pages/radar/Radar`（navigateTo）
- **Route**：`/pages/radar/Radar`。
- **Stores**：无（纯静态数据 `@/data/radar-data`、`@/data/radar-stack-data`）。
- **Key interactions**：主题 tab（书籍 / 7 个 stack）→ `setTheme`；书籍筛选（象限/环/搜索）；dot 点击 → 详情 modal；stack 维度列表。
- **Charts**：**原始 `<svg>` 雷达/气泡图**。书籍主题：4 象限轴 + 3 环 + 圆点（半径=`6 + rec*2.2`）；stack 主题：8 维雷达多边形 + 参考多边形。含 **8 轮力导向碰撞**算法（`adjustedDots`）。
- **Algorithm**：`dots` 布局（象限分桶、角度跨度、`ringR + layerOffset`）、碰撞检测（两两推开）、`stackPolygon` 外轮廓坐标。
- **⚠️ 原生不支持 SVG**，须 `<canvas type="2d">` 重写；碰撞模拟在 JS 中计算坐标后绘制即可。

## C-13 `pages/interview_bank/interview_bank`（navigateTo）
- **Route**：`/pages/interview_bank/interview_bank`。
- **Stores**：interviewBankStore（`viewed`/`bookmarks`）。静态数据 `@/data/interview_bank`（`algoBank`/`interviewBank`/`algoCategories`/`interviewCategories`）。
- **Key interactions**：tab（algorithm/interview）→ 分类 grid → `enterCategory`；难度 chip；搜索；题目卡 → 详情 modal（打开即 `markViewed`）；收藏 → `toggleBookmark`；LeetCode 链接 `window.open`（原生改 `wx.setClipboardData` 或复制 URL）。
- **Markdown**：面试题答案用 `renderMarkdown(answer)`（页内自实现：标题/列表/代码块/行内粗体代码）。可复用 `core/md.js`。

## C-14 `pages/five_year_plan/five_year_plan`（navigateTo）
- **Route**：`/pages/five_year_plan/five_year_plan`。
- **Stores**：fiveYearPlanStore。静态 `@/data/five_year_plan_config`（`YEAR_CONFIG`/`ALL_YEARS`/`CORE_PROJECTS`/`getAllItems`/`getGroupedItems`）。
- **Key interactions**：年/月导航；今日快速打卡 `toggleCheck`；日历格点击 → 编辑 modal `setDay`；`deleteDay`；`toggleLearning`；保存 `saveModal`。
- **Chart**：月历网格（**非 canvas**，纯 View 网格 + dot 着色），可直接用 WXML 复刻。
- **Algorithm**：`getStreak`/`getMonthPct`/`countDayChecks`（见 Part A-3）。

## C-15 `pages/learn_deep/learn_deep`（navigateTo，含 `:category` 子路由）
- **Route**：`/pages/learn_deep/learn_deep`（H5 还有 `/learn_deep/:category`，依赖 react-router `useParams`）。
- **Stores**：无（静态 `@/data/learn_deep_index`、`@/data/learn_deep_content`）。
- **Key interactions**：分类/搜索 → 文章列表 → 选中 slug → 右侧 `MarkdownRenderer` 渲染正文。
- **Markdown**：用 `MarkdownRenderer`（H5 版 react-markdown + 语法高亮；小程序版自实现 `parseMarkdown`）。**核心复用目标 `core/md.js`**。
- **⚠️ 数据量**：396 篇图解文章 + 长正文，原生须做**虚拟列表 + 懒渲染**；H5 双栏布局在手机端应改单栏。

## C-16 `pages/not-found/NotFound`
- **Route**：`/pages/not-found/NotFound`。无 store，纯 404 兜底页。

---

# Part D — 共享构建块

## D-1 静态数据 / 常量（`@/data/*`，⚠️ 多数缺失—见风险#1）
| 模块 | 提供内容 | 被谁使用 |
|------|----------|----------|
| `quiz-index` | `allQuizQuestions`、`quizByStack`、`quizStats`、`STACK_ORDER=['c','cpp','db','ds','linux','py','vdb']` | quizStore、index、quiz |
| `interview_bank` | `algoBank`、`interviewBank`、`algoCategories`、`interviewCategories`、`getAlgoByCategory`、`getInterviewBySource` | interview_bank 页 |
| `radar-data` | `BOOK_DATA`（41 本）、`QUADRANT_META`、`RING_META`、`ALL_QUADRANTS`、`ALL_RINGS`、`getBooksByQuadrant` | radar 页 |
| `radar-stack-data` | `STACK_RADARS`（7 栈图标/名称/颜色）、`STACK_DIMENSIONS`、`ALL_STACKS`、`buildStackRadar` | radar 页 |
| `five_year_plan_config` | `YEAR_CONFIG`(2026-2030 主题/核心/行动/extras)、`ALL_YEARS`、`CORE_PROJECTS`、`getAllItems`、`getGroupedItems`、`HabitItem` | five_year_plan 页、fiveYearPlanStore |
| `learn_deep_index` | `CATEGORIES`(9 类)、`learnDeepList`(396)、`getLearnDeepByCategory`、`getLearnDeepStats` | learn_deep 页 |
| `learn_deep_content` | `Record<slug, string>` 正文 | learn_deep 页 |
| `excerpt/initialExcerpts` | 4 演示 + 145 迁移摘抄 | excerptStore |

页面内联常量：index 的 `STACK_META`（7 栈 label/icon/color）、`ROADMAP_STAGES`（5 年阶段）、`SCORE_COLORS`；interview_tracker 的 `statusColors`/`statusProgress`；project_roadmap 的 `statusColors`/`priorityColors`；learning_path 的 `DIFFICULTY_COLORS`/`SUB_TYPE_*`；knowledge_graph 的 `DOMAIN_COLORS`/`DOMAIN_ICONS`；radar 的 `QUADRANT_META` 等。

## D-2 可复用组件
- **Charts**：`HeatmapCalendar`（GitHub 风格热力图，纯 div 网格，**可直接复用**）、`TrendChart`（H5 Recharts / MP 占位 div，**原生须 canvas 重写**）。
- **markdown**：`MarkdownRenderer`（H5：react-markdown + remark-gfm + 语法高亮）、`MarkdownRenderer.weapp`（自实现 `parseMarkdown`，输出 heading/code/image/blockquote/list/paragraph/inline-code/bold）。→ 直接作为 `core/md.js` 基础。
- **NavBar / TabBar**：H5 用 react-router 导航；MP 用 `app.config.ts` 的 tabBar 配置 + `wx.switchTab`。`TabBar` 组件内 `TAB_LIST` 与 `app.config` 保持一致（5 个 tab：首页/复习/面试/路径/设置）。
- **LazyLoad / Responsive / SafeArea**：布局辅助，原生按需简化。

## D-3 Services / Utils
- `services/dataService.ts`：Zustand 之上的 CRUD 封装（`KnowledgeService`/`ReviewService`/`InterviewService`/`LearningPathService`/`ExcerptService`/`ProjectService`）。⚠️ 引用了多个 store 上**不存在**的方法（如 `store.items`、`store.getReviewQueue`、`store.updateItem`、`store.reviewItem`、`store.totalReviews`、`store.masteredCount`、`updateExcerpt`、`updateStep`），是「理想化」封装，改写时若复用需对齐真实 store API。
- `services/exportService.ts`：导出逻辑（浏览器专用，见风险）。
- `utils/routes.ts`：`ROUTES`（页面路径常量）、`ROUTE_META`、`TAB_BAR_ROUTES`、`isTabBarPage`、`getTabBarIndex`。**原生改写的核心路由映射表**——tab 页用 `switchTab`，其余用 `navigateTo`。
- `utils/routerGuard.ts`：路由守卫。

---

# Part E — 风险 / 陷阱（移植到原生小程序）

1. **🔴 静态数据模块整体缺失（最高优先级）**：`@/data/*` 下 8 个模块（quiz 题库、excerpt 种子、interview_bank、radar-data/radar-stack-data、five_year_plan_config、learn_deep_index/content、initialExcerpts）在当前快照中**完全不存在**（`src/data/` 目录为空）。这些是 app 的「内容引擎」——尤其 `quiz-index`（全题库，驱动测验/统计/雷达）和 `learn_deep_content`（396 篇正文）。**改写前必须先定位并导出这些数据集**（可能在老项目 `project/reading-radar/` 或仓库其他位置），否则页面无内容可渲染。

2. **🔴 工厂抽象覆盖不全**：`createListStore`/`createObjectStore` 仅适配「简单列表 / 简单对象」。但 `dailyData`（嵌套 snapshots+dailyLogs+方法）、`fiveYearPlan`（三层嵌套树）、`quiz`（questions+answers+dailyPlans+stackProgress+currentSession）、`knowledge`（nodes+links+knowledgeGraph+gaps）、`notification`、`interviewBank` 均为**复杂/混合形态**，且多数 store 把 **UI 态/派生态**（`activePathId`、`currentSession`、`dueReviews`、`currentReview`、`currentIndex`、`loading`、`totalInterviews`、`unreadCount`、`filterBook` 等）与领域数据混在一起持久化。需要：①扩展工厂或写自定义 store；②把派生/UI 态剥离，不进 `db:` key。

3. **🔴 浏览器专用 API**：`settingsStore` 的 `exportData/importData/clearData`（localStorage、Blob、URL.createObjectURL、document.createElement、window.location.reload）、`Settings` 复制 API key、`Excerpt.handleCopy`、`interview_tracker.shareCompany` 的 `navigator.clipboard`、interview_bank 的 `window.open`。原生须改用 `wx.setClipboardData`、`wx.getFileSystemManager`、`wx.removeStorageSync`、文件导出走 `wx.saveFile` 等。

4. **🔴 SVG 图表无法直搬**：`knowledge_graph`（SVG 树 + 箭头 marker + 半径映射）与 `radar`（SVG 气泡/雷达 + 力导向碰撞）使用原始 `<svg>` DOM 与 `window.innerWidth/resize` 监听。原生小程序**不支持 SVG 节点**，必须用 `<canvas type="2d">` 在 JS 计算坐标后绘制（坐标/碰撞算法可复用）。`TrendChart` 的 Recharts 同理，H5 专用，原生须 canvas 重写（已有 MP 占位 div 可参考）。`HeatmapCalendar` 是纯 div 网格，可直接复用。

5. **🟠 路由机制差异**：H5 用 `window.history.pushState`+`popstate`、`react-router-dom`（`learn_deep` 的 `useNavigate`/`useParams`）。原生用 `wx.navigateTo`/`switchTab`/`redirectTo`。`utils/routes.ts` 的 `ROUTES` 是现成映射，但 `learn_deep/:category` 的子路由深度在原生需拆成独立页面或用页面参数（`?category=xxx`）。

6. **🟠 定时器**：`mock_interview` 用 `setInterval` 倒计时，页面卸载须 `onUnload` 清理，否则泄漏。

7. **🟠 Markdown 渲染体积**：H5 用 react-markdown + remark-gfm + 语法高亮（重）；小程序已有轻量 `parseMarkdown`。`learn_deep` 396 篇长文 + 雷达/图解大图，原生须**虚拟列表 + 图片懒加载**，否则卡顿/包体超限。

8. **🟠 大静态数据集进内存**：quiz 题库、radar 数据、learn_deep 正文全部内置，注意小程序**主包 2MB / 分包 20MB** 限制，建议把 `learn_deep_content` 等放分包或按需远程拉取。

9. **🟠 字段/命名冲突**：`projectStore` 必须重命名为 `db:study_projects` 以避开 PWB 的 portfolio `projects`；`interviewBankStore`（`viewed`/`bookmarks`）与 `interviewStore`（`companies`）是两个独立 store，迁移时勿混。

10. **🟠 持久化 key 与迁移**：旧 zustand persist name 无 `db:` 前缀（`quiz-storage`、`review-storage`、`settings-storage`、`excerpt-storage`、`rr2-*` 等）。新方案统一 `db:` 前缀，需要一次性迁移脚本或兼容读旧 key。

11. **🟠 内联 CSS 变量/动态 style**：多处 `style={{ '--accent': meta.color } as any}`、`document.documentElement.dataset.theme` 等。WXML 内联 style 不支持 CSS 变量，`theme` 切暗/亮应改用 class 或 `wx.setBackgroundColor`。

12. **🟠 死代码/未接功能**：`mock_interview`「导出面经」按钮无 handler；`shareToCommunity` 仅 `console.log`；`dataService.ts` 引用大量不存在的 store 方法；`MockInterview` 的 `mockAIService` 是规则 mock，真实 AI 需接入 `settings.apiConfig`（apiKey/apiUrl/model）走 `wx.request`。

13. **🟢 低风险（已确认可直移植）**：SM-2（Part B）、streak/盲点/错题本/每日计划等纯 JS 算法；emojis 在 `<text>` 中可用；ISO 日期 `new Date().toISOString().slice(0,10)` 在原生可用。

---
*生成方式：逐文件读取 `engineering/apps/web/knowledge_hub/src/` 全部 14 store + 16 页面目录 + components/services/utils，算法均逐字引用。所有 `db:` key 与 store 文件映射以设计文档为准。*
