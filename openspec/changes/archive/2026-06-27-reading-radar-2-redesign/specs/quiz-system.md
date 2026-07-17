# Quiz System 测验系统

## Overview

测验系统提供题库练习、错题本、AI 批改等功能，帮助用户检验学习效果。

## Features

### F4.1 题库浏览

**描述**：浏览所有题目的题库页面

**筛选维度**：
| 维度 | 选项 |
|------|------|
| 分类 | 全部、知识题、理解题、场景题、架构题、代码题 |
| 领域 | 全部、A编程基础、B后端工程、C AI理论、D LLM技术 |
| 难度 | 全部、⭐、⭐⭐、⭐⭐⭐、⭐⭐⭐⭐ |

**列表项显示**：
```
┌─────────────────────────────────────────────────────┐
│ [Python] [理解题] ⭐⭐⭐                            │
│                                                    │
│ 请解释 Python 中的 GIL 是什么...                     │
│                                                    │
│ 已学习: 12次  正确: 8次  掌握度: ████████░░ 80%   │
└─────────────────────────────────────────────────────┘
```

**排序**：
- 默认：按领域 + 难度
- 随机排序按钮
- 按掌握度排序（薄弱优先）

---

### F4.2 测验界面

**描述**：单题测验的交互界面

**布局**：
```
┌─────────────────────────────────────────────────────┐
│  刷题系统                          今日: 12/50 题  │
├─────────────────────────────────────────────────────┤
│ [全部] [知识] [理解] [场景] [架构] [代码]          │
│ [按领域 ▼] [按难度 ▼]  [🔀 随机]                  │
├─────────────────────────────────────────────────────┤
│ 题目 42/150                      ⏱️ 02:34        │
├─────────────────────────────────────────────────────┤
│ ┌───────────────────────────────────────────────┐ │
│ │ [Python] [理解题] ⭐⭐⭐                       │ │
│ │ [后端工程]                                     │ │
│ │                                               │ │
│ │ 请解释 Python 中的 GIL 是什么，               │ │
│ │ 它如何影响多线程编程？                        │ │
│ │                                               │ │
│ │ ┌─────────────────────────────────────────┐  │ │
│ │ │ 在此输入你的答案...                      │  │ │
│ │ │                                         │  │ │
│ │ │                                         │  │ │
│ │ └─────────────────────────────────────────┘  │ │
│ │                                               │ │
│ │ [🎤 语音输入]        [提交答案]              │ │
│ └───────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────┘
```

**交互流程**：
1. 显示题目
2. 用户输入/语音输入答案
3. 点击「提交」
4. AI 评分（如果有 API Key）→ 显示相似度 + 反馈
5. 显示参考答案
6. 用户自评正确/错误
7. 2.5 秒后自动下一题

---

### F4.3 语音输入

**描述**：支持语音输入答案

**实现**：
```typescript
const startListening = () => {
  const recognition = new (window.SpeechRecognition || 
                          window.webkitSpeechRecognition)();
  recognition.interimResults = true;
  recognition.continuous = true;
  recognition.onresult = (event) => {
    const transcript = event.results[0][0].transcript;
    setUserAnswer(transcript);
  };
  recognition.start();
};
```

**语音处理**：
- 去除语气词（呃、啊、嗯）
- 去除重复（"比如如" → "比如"）
- 清理多余空格

**UI 要求**：
- 麦克风按钮（录音中闪烁）
- 实时转文字显示

---

### F4.4 AI 批改（预埋）

**描述**：使用 LLM 批改答案

**接口设计**（预埋）：
```typescript
interface CompareResult {
  similarity: number;      // 0-100 相似度
  feedback: string;       // AI 点评
  isCorrect: boolean;     // 是否正确（相似度 >= 70）
}

// 接口调用
async function compareQuizAnswer(
  question: string,
  reference: string,
  userAnswer: string
): Promise<CompareResult>
```

**触发条件**：
- `localStorage.llmConfig` 存在 API Key
- 无 API Key 时：仅显示参考答案

---

### F4.5 统计面板

**描述**：显示当日/累计测验统计

**指标**：
| 指标 | 计算 | 显示 |
|------|------|------|
| 今日已学 | `dailyLog[TODAY].studied.length` | "12/50 题" |
| 累计已学 | 所有 `quizStats[].revealed` 计数 | 数字 |
| 正确率 | `correct / total * 100` | 百分比 |
| 连续天数 | 连续学习天数 | 数字 |

---

### F4.6 错题本

**描述**：错题/薄弱题回顾

**入口**：
- 导航栏「错题本」入口
- 仪表盘快捷入口

**筛选**：
- 全部错题
- 本周错题
- 未复习错题

**列表**：
```
┌─────────────────────────────────────────────────────┐
│ [Python] GIL 是什么？                               │
│ 你的答案：GIL 是全局解释器锁...                      │
│ 参考答案：GIL 是 Python 的全局解释器锁...            │
│ [再次练习] [查看解析]                               │
└─────────────────────────────────────────────────────┘
```

---

### F4.7 掌握度追踪

**描述**：追踪每道题的掌握情况

**掌握度规则**：
- 答对 5 次 → 标记为「已掌握」
- 正确率 = 累计正确 / 累计答题
- 遗忘曲线：30 天未复习自动降级

**数据结构**：
```typescript
interface QuizStats {
  [questionId: string]: {
    correctCount: number;     // 累计答对次数
    revealed: boolean;        // 是否查看过答案
    lastReviewed: string;     // YYYY-MM-DD
    userAnswers: AnswerRecord[];
    aiResults: AIRecord[];
  };
}
```

## Data Models

```typescript
// 题目
interface QuizQuestion {
  id: string;
  domain: string;
  category: '知识题' | '理解题' | '场景题' | '架构题' | '代码题';
  difficulty: 1 | 2 | 3 | 4;
  topic?: string;
  question: string;
  answer: string;
  explanation?: string;
}

// 答题记录
interface AnswerRecord {
  answer: string;
  date: string;
  timestamp: string;
}
```

## Technical Notes

- 语音识别：Web Speech API（需要 HTTPS 或 localhost）
- AI 批改：预留接口，可接入 OpenAI/DeepSeek
- 小程序：使用微信同声传译插件
- 状态管理：Zustand + persist
