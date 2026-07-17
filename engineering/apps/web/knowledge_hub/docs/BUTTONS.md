# Reading Radar 2.0 — 全页面可点击按钮清单

> 最后更新：2026-06-23 · 配套自动化测试用例设计依据

## 使用说明

每个页面列出**所有可点击元素**（含 `<View onClick>` / `<Button onClick>` / `<Switch onChange>` / `<Input>` / 卡片可点击区域），格式：

| 序号 | 元素 | 位置 | 行为 | 断言 |
|------|------|------|------|------|

**全表覆盖 13 个页面 + Layout**，自动化测试必须对每个按钮设计 1+ 个断言。

---

## 0. Layout（全局）

### 0.1 TopBar 顶栏
| 序号 | 元素 | 位置 | 行为 | 断言 |
|------|------|------|------|------|
| L01 | 🌙 主题切换按钮 | TopBar 右侧 | 切换 dark/light 主题，documentElement.dataset.theme 变化 | localStorage 写入 / DOM 属性切换 |
| L02 | 🔔 通知按钮 | TopBar | 展开/收起通知下拉，动态通知数 ≥ 0 | 下拉显示 / 通知数 |
| L03 | 👤 用户头像 | TopBar | 弹出下拉：编辑昵称/退出登录 | 下拉显示 / 跳转 |
| L04 | 搜索框 | TopBar 中部 | 搜索题目/路径/摘抄（占位功能） | 输入有响应 |

### 0.2 Sidebar 侧栏
| 序号 | 元素 | 位置 | 行为 | 断言 |
|------|------|------|------|------|
| L05-L16 | 12 个 nav-item | Sidebar | 点击切换页面 | URL 变化 / active 高亮 |
| L17 | 连续学习天数卡片 | Sidebar 底部 | 显示 streak（从 fiveYearPlanStore.getStreak()） | 数字 = store 值 |
| L18 | 用户邮箱/简介 | Sidebar 底部 | 静态展示 | 文本含 user.email |

---

## 1. 仪表盘 (Dashboard `/`)

| 序号 | 元素 | 位置 | 行为 | 断言 |
|------|------|------|------|------|
| D01 | 五年计划今日打卡 chip | "当前进程"卡 | 仅显示（无 onClick） | 8 个 chip 渲染 |
| D02 | 查看详细计划链接 | plan-card 底部 | navigate('/five_year_plan') | URL 变化 |
| D03 | 知识点总数 stat-card | 总览 5 卡 | 仅显示 | 数字 = totals.totalItems |
| D04 | 已测知识点 stat-card | 总览 5 卡 | 仅显示 | 数字 + 覆盖率 |
| D05 | 平均测验分 stat-card | 总览 5 卡 | 仅显示 | 数字 = totals.avgScore |
| D06 | 已掌握 stat-card | 总览 5 卡 | 仅显示 | 数字 + 掌握率 |
| D07 | 累计测验次数 stat-card | 总览 5 卡 | 仅显示 | 数字 = totals.totalAttempts |
| D08-D14 | 7 个技术栈进度卡 | stacks-grid | navigate('/quiz') | 跳转到 /quiz |
| D15 | 高估盲点项 | 盲点列表 | navigate('/quiz') | 跳转 |
| D16 | 评测系统链接 | 空状态文字 | navigate('/quiz') | 跳转 |
| D17-D21 | 5 个 roadmap-stage | 五年成长路线 | navigate('/five_year_plan') | 跳转 + active 高亮 |
| D22 | 查看完整计划链接 | 路线卡底部 | navigate('/five_year_plan') | 跳转 |

**总按钮数：22**

---

## 2. 题库练习 (Quiz `/quiz`)

### 2.1 plan 模式（默认）
| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| Q01 | 开始今日计划 | startSession → 跳 quiz 模式 | mode='quiz' / currentSession 存在 |
| Q02 | 换一批 | generateDailyPlan() | plan.questionIds 变化 |
| Q03 | 浏览题库 entry | setMode('list') | mode='list' |
| Q04 | 错题本 entry | setMode('wrong') | mode='wrong' |
| Q05 | 错题重做 entry | startWrongQuiz → quiz | currentSession |
| Q06 | 学习统计 entry | setMode('stats') | mode='stats' |
| Q07-Q13 | 7 个 stack-card | setFilter(stack) + setMode('list') | 跳 list + 预选 stack |

### 2.2 list 模式
| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| Q14 | Stack chip（全部 + 7 stack）| setFilter | filter.stack 更新 |
| Q15 | 分类 chip（动态） | setFilter | filter.category 更新 |
| Q16 | 难度 chip（动态） | setFilter | filter.difficulty 更新 |
| Q17 | 开始前 20 题 | startListQuiz | currentSession.length=20 |
| Q18 | 折叠卡 item-summary | toggleExpand(q.id) | expandedQuestions 增删 |
| Q19 | 直接答此题 | startSession([q.id]) | 进入 quiz 模式 |
| Q20 | 返回计划 | setMode('plan') | mode='plan' |

### 2.3 quiz 模式
| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| Q21 | ✕ 退出 | endSession → plan | mode='plan' |
| Q22 | 4 个选项 | setUserAnswer(letter) | userAnswer 更新 |
| Q23 | Textarea 答题区 | setUserAnswer(e.detail.value) | userAnswer 更新 |
| Q24 | 提交答案 | submitAnswer | isAnswered=true |
| Q25 | 上一题 | prevQuestion | currentIndex -=1 |
| Q26 | 下一题 / 完成 | nextQuestion / endSession | currentIndex +=1 或回 plan |

### 2.4 wrong 模式
| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| Q27 | 重做错题（前 10 道） | startWrongQuiz | currentSession |
| Q28 | 返回计划 | setMode('plan') | mode='plan' |

### 2.5 stats 模式
| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| Q29 | 返回计划 | setMode('plan') | mode='plan' |

**总按钮数：29**

---

## 3. 复习计划 (Review `/review`)

| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| R01 | 待复习 stat-card | setTab('due') | tab='due' + active 高亮 |
| R02 | 7 天内 stat-card | setTab('all') | tab='all' + active |
| R03 | 已延期 stat-card | setTab('all') | tab='all' + active |
| R04 | 已掌握 stat-card | setTab('done') | tab='done' + active |
| R05 | 待复习 tab | setTab('due') | tab='due' + active |
| R06 | 全部计划 tab | setTab('all') | tab='all' + active |
| R07 | 已完成 tab | setTab('done') | tab='done' + active |
| R08 | 评分按钮 q=0（忘记） | rateReview(0) | record 更新 |
| R09 | 评分按钮 q=1（难） | rateReview(1) | record 更新 |
| R10 | 评分按钮 q=2（较难） | rateReview(2) | record 更新 |
| R11 | 评分按钮 q=3（一般） | rateReview(3) | record 更新 |
| R12 | 评分按钮 q=4（较易） | rateReview(4) | record 更新 |
| R13 | 评分按钮 q=5（简单） | rateReview(5) | record 更新 |
| R14 | 继续下一轮 | nextReview | currentReview 切换 |

**总按钮数：14**

---

## 4. 面试追踪 (Interview Tracker `/interview_tracker`)

| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| T01 | + 添加公司按钮 | setShowForm(true) | 表单显示 |
| T02 | 14 个状态 chip | setFormData.status | formData 更新 |
| T03 | 提交新公司 | handleAddCompany | store.companies +1 |
| T04 | 分享按钮 | shareCompany | navigator.clipboard 写入 |
| T05 | 删除按钮 | handleRemove | store.companies -1 |
| T06-T19 | 14 个状态切换 chip | handleStatusChange | company.status 更新 |
| T20 | 取消/收起表单 | setShowForm(false) | 表单隐藏 |

**总按钮数：20**

---

## 5. 模拟面试 (Mock Interview `/mock_interview`)

| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| M01 | 角色 chip（前/后端/算法等）| setConfig.role | config.role 更新 |
| M02 | 难度 chip | setConfig.difficulty | config.difficulty 更新 |
| M03 | 题数 chip | setConfig.questions | config.questions 更新 |
| M04 | 启用 AI Switch | toggle enableAI | config.enableAI 切换 |
| M05 | 开始面试 | startInterview | currentQuestion 出现 |
| M06 | 跳过追问 | skipFollowUps | 跳下一题 |
| M07 | 上一题 | handlePrev | currentIndex -=1 |
| M08 | 下一题 | handleNext | currentIndex +=1 |
| M09 | 结束面试（结果页）| 退到配置 | mode 切换 |
| M10 | 重新开始（结果页）| 退到配置 | mode 切换 |

**总按钮数：10**

---

## 6. 面试题库 (Interview Bank `/interview_bank`)

| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| B01 | 分类卡（category）| enterCategory | mode='list' + category 过滤 |
| B02 | 算法 tab | switchTab('algorithm') | tab='algorithm' |
| B03 | 面试 tab | switchTab('interview') | tab='interview' |
| B04 | 题目卡 | setDetailItem(q) | 详情弹窗显示 |
| B05 | 难度 chip | setDifficulty | difficulty 过滤 |
| B06 | 收藏按钮 | toggleBookmark | detailItem.bookmarked 切换 |
| B07 | 关闭按钮 | setDetailItem(null) | 弹窗关闭 |
| B08 | 返回主页 | backToHome | 回到分类网格 |
| B09 | 算法题链接 | window.open | 新标签页打开 |
| B10 | 模态遮罩 | setDetailItem(null) | 关闭弹窗 |

**总按钮数：10**

---

## 7. 知识图谱 (Knowledge Graph `/knowledge_graph`)

| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| K01 | 节点（14 个）| setSelectedId(p.id) | 详情弹窗 |
| K02 | 关闭模态 | setSelectedId(null) | 弹窗关闭 |
| K03 | 5 个 level 按钮（0-4）| updateNodeLevel | node.level 更新 |
| K04 | 减少 level 按钮 | adjustLevel(-1) | level -=1 |
| K05 | 增加 level 按钮 | adjustLevel(+1) | level +=1 |
| K06 | 关联节点跳转 | setSelectedId(other.id) | 弹窗内容更新 |
| K07 | 模态遮罩 | setSelectedId(null) | 关闭 |

**总按钮数：7**

---

## 8. 学习路径 (Learning Path `/learning_path`)

| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| P01 | 3 条路径 tab | setActivePath | activePath 切换 |
| P02 | step 头部 | toggleExpand(step.id) | 折叠/展开 |
| P03 | sub-step checkbox | toggleSubStep | completedSubSteps 切换 |
| P04 | 完成本步骤按钮 | nextStep | currentStep +=1 |
| P05 | 上一步 | prevStep | currentStep -=1 |
| P06 | 重置 | setShowReset(true) | 模态显示 |
| P07 | 确认重置 | resetPath | currentStep=0 |
| P08 | 取消重置 | setShowReset(false) | 模态关闭 |
| P09 | 模态遮罩 | setShowReset(false) | 模态关闭 |

**总按钮数：9**

---

## 9. 图解系列 (Learn Deep `/learn_deep`)

### 9.1 双栏布局（默认）
| 序号 | 元素 | 位置 | 行为 | 断言 |
|------|------|------|------|------|
| L01 | 搜索框 | 顶栏 | setSearchQuery | filtered 变化 |
| L02 | 分类标题 | 侧边栏 | toggleCategory | 折叠/展开 |
| L03 | 文章项 | 侧边栏 | setSelectedSlug | 右侧显示文章 |
| L04 | 面包屑-图解系列 | 阅读器 | setSelectedSlug(null) | 返回列表 |
| L05 | 面包屑-分类名 | 阅读器 | setSelectedCategory + 返回 | 切换分类 |
| L06 | 上一篇 | 阅读器底部 | setSelectedSlug(prev) | 切换到 prev |
| L07 | 下一篇 | 阅读器底部 | setSelectedSlug(next) | 切换到 next |

### 9.2 列表模式（小程序端）
| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| LD01 | 分类 chip | setCategory | category 切换 |
| LD02 | 搜索框 | setSearch | filtered 变化 |
| LD03 | 文章卡 | openDetail | mode='detail' |
| LD04 | 返回列表 | setMode('list') | mode='list' |
| LD05 | 面包屑分类 | setCategory + setMode('list') | 列表 + 预选 |
| LD06 | 上一篇 | setSelectedSlug(prev) | 切到 prev |
| LD07 | 下一篇 | setSelectedSlug(next) | 切到 next |

**总按钮数：14（双栏7 + 列表7）**

---

## 10. 摘抄管理 (Excerpt `/excerpt`)

| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| X01 | 搜索框 | setSearchQuery | filtered 变化 |
| X02 | 书籍 filter chip | setFilterBook | 按书筛选 |
| X03 | 标签 filter chip | setFilterTag | 按标签筛选 |
| X04 | 年份 filter chip | setFilterYear | 按年筛选 |
| X05 | 添加/取消按钮 | setShowAddForm | 表单显示切换 |
| X06 | 摘抄内容输入 | setNewExcerpt | 表单内容 |
| X07 | 书籍名称输入 | setNewExcerpt | 表单书籍 |
| X08 | 标签输入 | setNewExcerpt | 表单标签 |
| X09 | 笔记输入 | setNewExcerpt | 表单笔记 |
| X10 | 保存摘抄 | handleAdd | store.excerpts +1 |
| X11 | 摘抄卡 | openDetail | 详情弹窗 |
| X12 | 收藏按钮 | toggleFavorite | favorite 切换 |
| X13 | 状态徽章 | cycleStatus | status 切换 |
| X14 | 保存笔记 | saveNote | note 更新 |
| X15 | 删除按钮 | deleteExcerpt | excerpts -1 |
| X16 | 关闭模态 | setSelectedExcerpt(null) | 弹窗关闭 |

**总按钮数：16**

---

## 11. 差距分析 (Gap Analysis `/gap_analysis`)

| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| G01 | 开始答题 CTA | navigateTo('/quiz') | 跳转 |
| G02 | 5 个 gap-item（仅显示） | - | 渲染 |

**总按钮数：2**

---

## 12. 技术雷达 (Radar `/radar`)

| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| RA01 | 8 个主题 tab（书籍 + 7 stack）| setTheme | 主题切换 |
| RA02 | 4 个象限 chip | setActiveQuad | 按象限过滤 |
| RA03 | 3 个环 chip | setActiveRing | 按环过滤 |
| RA04 | 圆点 | setSelected | 详情弹窗 |
| RA05 | 关闭模态 | setSelected(null) | 关闭 |
| RA06 | 模态遮罩 | setSelected(null) | 关闭 |

**总按钮数：6**

---

## 13. 五年计划 (Five Year Plan `/five_year_plan`)

| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| F01 | 5 个年份 chip | switchYear | currentYear 切换 |
| F02 | 每日打卡项 | quickToggle | checks 切换 |
| F03 | 编辑按钮 | openModal | 模态显示 |
| F04 | 上一月 | changeMonth(-1) | 月份切换 |
| F05 | 下一月 | changeMonth(+1) | 月份切换 |
| F06 | 回到今天 | goToToday | 跳到今天 |
| F07 | 月历单元格 | openModal | 模态显示 |
| F08 | 模态遮罩 | closeModal | 模态关闭 |
| F09 | 模态内打卡项 | toggleEditCheck | checks 切换 |
| F10 | 学习开关 | toggleEditHasLearning | learning 切换 |
| F11 | 删除日期 | onDelete | 数据删除 |
| F12 | 关闭模态 | closeModal | 模态关闭 |
| F13 | 保存模态 | saveModal | 数据保存 |

**总按钮数：13**

---

## 14. 项目路线 (Project Roadmap `/project_roadmap`)

| 序号 | 元素 | 行为 | 断言 |
|------|------|------|------|
| PR01 | 添加项目按钮 | setShowForm | 表单显示 |
| PR02 | 状态 chip | setFormData.status | status 更新 |
| PR03 | 优先级 chip | setFormData.priority | priority 更新 |
| PR04 | 提交新项目 | handleAdd | store +1 |
| PR05 | 项目卡 | setSelectedProject | 详情弹窗 |
| PR06 | 关闭模态 | setSelectedProject(null) | 关闭 |
| PR07 | 模态遮罩 | setSelectedProject(null) | 关闭 |
| PR08 | 项目内操作按钮 1 | - | 视实现 |
| PR09 | 项目内操作按钮 2 | - | 视实现 |

**总按钮数：9**

---

## 汇总

| 页面 | 按钮数 |
|------|--------|
| Layout (Sidebar + TopBar) | 18 |
| Dashboard | 22 |
| Quiz | 29 |
| Review | 14 |
| Interview Tracker | 20 |
| Mock Interview | 10 |
| Interview Bank | 10 |
| Knowledge Graph | 7 |
| Learning Path | 9 |
| Learn Deep | 14 |
| Excerpt | 16 |
| Gap Analysis | 2 |
| Radar | 6 |
| Five Year Plan | 13 |
| Project Roadmap | 9 |
| **合计** | **199** |

---

## 自动化测试覆盖策略

1. **每个按钮至少 1 个 happy path 断言**
2. **关键路径加 error path**（如空状态、越界）
3. **持久化断言**：localStorage 写入验证（复习/设置/摘抄）
4. **导航断言**：URL 变化 + 目标页面元素渲染
5. **优先级排序**：
   - P0：Layout + Dashboard + Quiz + Review + Interview Tracker + Interview Bank + Settings（核心 CRUD）
   - P1：Mock Interview + Knowledge Graph + Learning Path + Learn Deep + Excerpt
   - P2：Gap Analysis + Radar + Five Year Plan + Project Roadmap

## 关联 spec 文件

| 页面 | E2E spec |
|------|----------|
| Dashboard | e2e/dashboard.spec.ts（扩） |
| Quiz | e2e/quiz.spec.ts（新建） |
| Review | e2e/review.spec.ts（扩） |
| Interview Tracker | e2e/interview_tracker-full.spec.ts（新建） |
| Mock Interview | e2e/mock_interview-full.spec.ts（新建） |
| Interview Bank | e2e/interview_bank-full.spec.ts（新建） |
| Knowledge Graph | e2e/knowledge_graph.spec.ts（新建） |
| Learning Path | e2e/learning_path.spec.ts（新建） |
| Learn Deep | e2e/learn_deep.spec.ts（新建） |
| Excerpt | e2e/excerpt.spec.ts（新建） |
| Gap Analysis | e2e/gap_analysis.spec.ts（新建） |
| Radar | e2e/radar.spec.ts（新建） |
| Five Year Plan | e2e/five_year_plan.spec.ts（新建） |
| Project Roadmap | e2e/project_roadmap.spec.ts（新建） |
| Settings | e2e/settings.spec.ts（新建） |
| Layout | e2e/topbar-layout.spec.ts（新建） |
| **共 16 个 spec** | |
