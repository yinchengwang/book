"use strict";

/**
 * 扩充 practice-data.js 到约 4 倍题量
 * 运行：node scripts/expand-practice-data.js
 * 输出覆盖 data/app/practice-data.js
 */

const fs = require("fs");
const path = require("path");

// ================================================================
// 各分类扩充题集
// ================================================================

// ── 辅助：按 difficulty 排序 ──
function byDiff(a, b) {
  var order = { easy: 0, medium: 1 };
  return (order[a.difficulty] || 1) - (order[b.difficulty] || 1);
}

// ── 已存在于阶段 1 中的题号 ──
var EXISTING_LC = new Set([
  1, 2, 3, 5, 11, 14, 15, 18, 19, 20, 21, 26, 27, 31, 34, 35, 48, 49, 54, 56,
  66, 71, 83, 102, 125, 138, 139, 141, 142, 143, 148, 150, 155, 160, 169, 189,
  205, 206, 225, 234, 237, 239, 242, 279, 283, 344, 347, 383, 387, 394, 434,
  503, 567, 621, 622, 641, 739, 844, 876, 933, 946, 994,
]);
var EXISTING_JZ = new Set(["JZ03", "JZ06", "JZ09", "JZ25", "JZ29", "JZ30"]);

function isExisting(platform, id) {
  if (platform === "leetcode") return EXISTING_LC.has(Number(id));
  if (platform === "nowcoder") return EXISTING_JZ.has(String(id));
  return false;
}

// ── 各分类扩充数据 ──

function lc(title, id, diff, tags) {
  return {
    title: String(title),
    platform: "leetcode",
    problemId: Number(id),
    difficulty: String(diff),
    tags: Array.isArray(tags) ? tags : [tags],
  };
}
function jz(title, id, diff, tags) {
  return {
    title: String(title),
    platform: "nowcoder",
    problemId: String(id),
    difficulty: String(diff),
    tags: Array.isArray(tags) ? tags : [tags],
  };
}
function maybeRepeat(item) {
  if (isExisting(item.platform, item.problemId)) item.repeat = true;
  return item;
}

// ================================================================
// 阶段 1：基础数据结构
// ================================================================

var phase1 = [];

// ── 1.1 数组 ── (18 → ~50)
phase1.push({
  id: "array",
  title: "数组",
  phase: 1,
  order: 1,
  icon: "📗",
  desc: "数组是最基本的数据结构，连续内存存储、随机访问 O(1)。掌握数组的遍历、原地修改、双指针遍历是算法入门的第一步。",
  items: [
    // Easy —— 保留原有 10 个 + 扩充
    {
      title: "两数之和",
      platform: "leetcode",
      problemId: 1,
      difficulty: "easy",
      tags: ["哈希表"],
      repeat: true,
    },
    {
      title: "移除元素",
      platform: "leetcode",
      problemId: 27,
      difficulty: "easy",
      tags: ["双指针"],
    },
    {
      title: "搜索插入位置",
      platform: "leetcode",
      problemId: 35,
      difficulty: "easy",
      tags: ["二分查找"],
      repeat: true,
    },
    {
      title: "删除有序数组中的重复项",
      platform: "leetcode",
      problemId: 26,
      difficulty: "easy",
      tags: ["双指针"],
    },
    {
      title: "加一",
      platform: "leetcode",
      problemId: 66,
      difficulty: "easy",
      tags: ["数学"],
    },
    {
      title: "轮转数组",
      platform: "leetcode",
      problemId: 189,
      difficulty: "easy",
      tags: ["翻转"],
    },
    {
      title: "多数元素",
      platform: "leetcode",
      problemId: 169,
      difficulty: "easy",
      tags: ["投票", "哈希表"],
    },
    {
      title: "移动零",
      platform: "leetcode",
      problemId: 283,
      difficulty: "easy",
      tags: ["双指针"],
    },
    {
      title: "数组中重复的数字",
      platform: "nowcoder",
      problemId: "JZ03",
      difficulty: "easy",
      tags: ["哈希表"],
    },
    {
      title: "顺时针打印矩阵",
      platform: "nowcoder",
      problemId: "JZ29",
      difficulty: "easy",
      tags: ["模拟", "矩阵"],
    },
    // 新增 Easy
    {
      title: "合并两个有序数组",
      platform: "leetcode",
      problemId: 88,
      difficulty: "easy",
      tags: ["双指针", "从后往前"],
    },
    {
      title: "杨辉三角",
      platform: "leetcode",
      problemId: 118,
      difficulty: "easy",
      tags: ["模拟"],
    },
    {
      title: "杨辉三角 II",
      platform: "leetcode",
      problemId: 119,
      difficulty: "easy",
      tags: ["模拟", "空间优化"],
    },
    {
      title: "买卖股票的最佳时机",
      platform: "leetcode",
      problemId: 121,
      difficulty: "easy",
      tags: ["DP", "贪心"],
    },
    {
      title: "两数之和 II - 输入有序数组",
      platform: "leetcode",
      problemId: 167,
      difficulty: "easy",
      tags: ["对撞指针"],
    },
    {
      title: "存在重复元素",
      platform: "leetcode",
      problemId: 217,
      difficulty: "easy",
      tags: ["哈希表", "排序"],
    },
    {
      title: "汇总区间",
      platform: "leetcode",
      problemId: 228,
      difficulty: "easy",
      tags: ["遍历"],
    },
    {
      title: "丢失的数字",
      platform: "leetcode",
      problemId: 268,
      difficulty: "easy",
      tags: ["位运算", "数学"],
    },
    {
      title: "区域和检索 - 数组不可变",
      platform: "leetcode",
      problemId: 303,
      difficulty: "easy",
      tags: ["前缀和"],
    },
    {
      title: "两个数组的交集",
      platform: "leetcode",
      problemId: 349,
      difficulty: "easy",
      tags: ["哈希集合"],
    },
    {
      title: "两个数组的交集 II",
      platform: "leetcode",
      problemId: 350,
      difficulty: "easy",
      tags: ["哈希表", "双指针"],
    },
    {
      title: "第三大的数",
      platform: "leetcode",
      problemId: 414,
      difficulty: "easy",
      tags: ["遍历"],
    },
    {
      title: "找到所有数组中消失的数字",
      platform: "leetcode",
      problemId: 448,
      difficulty: "easy",
      tags: ["原地标记"],
    },
    {
      title: "最大连续 1 的个数",
      platform: "leetcode",
      problemId: 485,
      difficulty: "easy",
      tags: ["遍历"],
    },
    {
      title: "数组拆分",
      platform: "leetcode",
      problemId: 561,
      difficulty: "easy",
      tags: ["排序", "贪心"],
    },
    {
      title: "种花问题",
      platform: "leetcode",
      problemId: 605,
      difficulty: "easy",
      tags: ["贪心"],
    },
    {
      title: "三个数的最大乘积",
      platform: "leetcode",
      problemId: 628,
      difficulty: "easy",
      tags: ["数学", "排序"],
    },
    {
      title: "子数组最大平均数 I",
      platform: "leetcode",
      problemId: 643,
      difficulty: "easy",
      tags: ["滑动窗口", "定长"],
    },
    {
      title: "最长连续递增序列",
      platform: "leetcode",
      problemId: 674,
      difficulty: "easy",
      tags: ["遍历"],
    },
    {
      title: "数组的度",
      platform: "leetcode",
      problemId: 697,
      difficulty: "easy",
      tags: ["哈希表"],
    },
    {
      title: "寻找数组的中心下标",
      platform: "leetcode",
      problemId: 724,
      difficulty: "easy",
      tags: ["前缀和"],
    },
    {
      title: "托普利茨矩阵",
      platform: "leetcode",
      problemId: 766,
      difficulty: "easy",
      tags: ["遍历", "矩阵"],
    },
    {
      title: "较大分组的位置",
      platform: "leetcode",
      problemId: 830,
      difficulty: "easy",
      tags: ["遍历"],
    },
    {
      title: "转置矩阵",
      platform: "leetcode",
      problemId: 867,
      difficulty: "easy",
      tags: ["矩阵"],
    },
    {
      title: "单调数列",
      platform: "leetcode",
      problemId: 896,
      difficulty: "easy",
      tags: ["遍历"],
    },
    {
      title: "按奇偶排序数组",
      platform: "leetcode",
      problemId: 905,
      difficulty: "easy",
      tags: ["双指针"],
    },
    {
      title: "按奇偶排序数组 II",
      platform: "leetcode",
      problemId: 922,
      difficulty: "easy",
      tags: ["双指针"],
    },
    {
      title: "有效的山脉数组",
      platform: "leetcode",
      problemId: 941,
      difficulty: "easy",
      tags: ["遍历"],
    },
    {
      title: "有序数组的平方",
      platform: "leetcode",
      problemId: 977,
      difficulty: "easy",
      tags: ["双指针"],
    },
    {
      title: "数组形式的整数加法",
      platform: "leetcode",
      problemId: 989,
      difficulty: "easy",
      tags: ["模拟"],
    },
    // Medium —— 保留原有 8 个 + 扩充
    {
      title: "三数之和",
      platform: "leetcode",
      problemId: 15,
      difficulty: "medium",
      tags: ["排序", "双指针"],
    },
    {
      title: "盛最多水的容器",
      platform: "leetcode",
      problemId: 11,
      difficulty: "medium",
      tags: ["双指针"],
    },
    {
      title: "合并区间",
      platform: "leetcode",
      problemId: 56,
      difficulty: "medium",
      tags: ["排序", "区间"],
    },
    {
      title: "下一个排列",
      platform: "leetcode",
      problemId: 31,
      difficulty: "medium",
      tags: ["找规律"],
    },
    {
      title: "螺旋矩阵",
      platform: "leetcode",
      problemId: 54,
      difficulty: "medium",
      tags: ["模拟", "矩阵"],
    },
    {
      title: "在排序数组中查找元素的第一个和最后一个位置",
      platform: "leetcode",
      problemId: 34,
      difficulty: "medium",
      tags: ["二分查找"],
    },
    {
      title: "四数之和",
      platform: "leetcode",
      problemId: 18,
      difficulty: "medium",
      tags: ["排序", "双指针"],
    },
    {
      title: "旋转图像",
      platform: "leetcode",
      problemId: 48,
      difficulty: "medium",
      tags: ["矩阵", "翻转"],
    },
    // 新增 Medium
    {
      title: "组合总和",
      platform: "leetcode",
      problemId: 39,
      difficulty: "medium",
      tags: ["回溯", "DFS"],
    },
    {
      title: "组合总和 II",
      platform: "leetcode",
      problemId: 40,
      difficulty: "medium",
      tags: ["回溯", "剪枝"],
    },
    {
      title: "矩阵置零",
      platform: "leetcode",
      problemId: 73,
      difficulty: "medium",
      tags: ["矩阵", "原地标记"],
    },
    {
      title: "子集",
      platform: "leetcode",
      problemId: 78,
      difficulty: "medium",
      tags: ["回溯", "位运算"],
    },
    {
      title: "删除有序数组中的重复项 II",
      platform: "leetcode",
      problemId: 80,
      difficulty: "medium",
      tags: ["快慢指针"],
    },
    {
      title: "子集 II",
      platform: "leetcode",
      problemId: 90,
      difficulty: "medium",
      tags: ["回溯", "剪枝"],
    },
    {
      title: "买卖股票的最佳时机 II",
      platform: "leetcode",
      problemId: 122,
      difficulty: "medium",
      tags: ["贪心"],
    },
    {
      title: "加油站",
      platform: "leetcode",
      problemId: 134,
      difficulty: "medium",
      tags: ["贪心", "遍历"],
    },
    {
      title: "乘积最大子数组",
      platform: "leetcode",
      problemId: 152,
      difficulty: "medium",
      tags: ["DP"],
    },
    {
      title: "寻找峰值",
      platform: "leetcode",
      problemId: 162,
      difficulty: "medium",
      tags: ["二分查找"],
    },
    {
      title: "长度最小的子数组",
      platform: "leetcode",
      problemId: 209,
      difficulty: "medium",
      tags: ["滑动窗口", "前缀和"],
    },
    {
      title: "除自身以外数组的乘积",
      platform: "leetcode",
      problemId: 238,
      difficulty: "medium",
      tags: ["前缀和", "空间优化"],
    },
    {
      title: "H 指数",
      platform: "leetcode",
      problemId: 274,
      difficulty: "medium",
      tags: ["排序", "计数"],
    },
    {
      title: "寻找重复数",
      platform: "leetcode",
      problemId: 287,
      difficulty: "medium",
      tags: ["快慢指针", "位运算"],
    },
    {
      title: "生命游戏",
      platform: "leetcode",
      problemId: 289,
      difficulty: "medium",
      tags: ["矩阵", "原地标记"],
    },
    {
      title: "递增的三元子序列",
      platform: "leetcode",
      problemId: 334,
      difficulty: "medium",
      tags: ["贪心"],
    },
    {
      title: "前 K 个高频元素",
      platform: "leetcode",
      problemId: 347,
      difficulty: "medium",
      tags: ["哈希表", "桶排序"],
    },
    {
      title: "O(1) 时间插入、删除和获取随机元素",
      platform: "leetcode",
      problemId: 380,
      difficulty: "medium",
      tags: ["设计", "哈希表"],
    },
    {
      title: "打乱数组",
      platform: "leetcode",
      problemId: 384,
      difficulty: "medium",
      tags: ["随机", "设计"],
    },
    {
      title: "数组中重复的数据",
      platform: "leetcode",
      problemId: 442,
      difficulty: "medium",
      tags: ["原地标记"],
    },
    {
      title: "四数相加 II",
      platform: "leetcode",
      problemId: 454,
      difficulty: "medium",
      tags: ["哈希表", "分组"],
    },
    {
      title: "对角线遍历",
      platform: "leetcode",
      problemId: 498,
      difficulty: "medium",
      tags: ["矩阵", "模拟"],
    },
    {
      title: "连续的子数组和",
      platform: "leetcode",
      problemId: 523,
      difficulty: "medium",
      tags: ["前缀和", "哈希表"],
    },
    {
      title: "连续数组",
      platform: "leetcode",
      problemId: 525,
      difficulty: "medium",
      tags: ["前缀和", "哈希表"],
    },
    {
      title: "和为 K 的子数组",
      platform: "leetcode",
      problemId: 560,
      difficulty: "medium",
      tags: ["前缀和", "哈希映射"],
    },
    {
      title: "最短无序连续子数组",
      platform: "leetcode",
      problemId: 581,
      difficulty: "medium",
      tags: ["遍历", "排序"],
    },
    {
      title: "任务调度器",
      platform: "leetcode",
      problemId: 621,
      difficulty: "medium",
      tags: ["贪心", "计数"],
    },
    {
      title: "岛屿的最大面积",
      platform: "leetcode",
      problemId: 695,
      difficulty: "medium",
      tags: ["DFS", "矩阵"],
    },
  ],
});

// ── 1.2 字符串 ── (14 → ~50)
phase1.push({
  id: "string",
  title: "字符串",
  phase: 1,
  order: 2,
  icon: "📗",
  desc: "字符串操作是编程基本功，涉及遍历、匹配、翻转、子串处理。字符串问题常与哈希表、双指针、滑动窗口结合考察。",
  items: [
    // Easy
    {
      title: "最长公共前缀",
      platform: "leetcode",
      problemId: 14,
      difficulty: "easy",
      tags: ["字符串"],
    },
    {
      title: "反转字符串",
      platform: "leetcode",
      problemId: 344,
      difficulty: "easy",
      tags: ["双指针"],
    },
    {
      title: "字符串中的第一个唯一字符",
      platform: "leetcode",
      problemId: 387,
      difficulty: "easy",
      tags: ["哈希表", "计数"],
    },
    {
      title: "赎金信",
      platform: "leetcode",
      problemId: 383,
      difficulty: "easy",
      tags: ["哈希表", "计数"],
    },
    {
      title: "有效的字母异位词",
      platform: "leetcode",
      problemId: 242,
      difficulty: "easy",
      tags: ["哈希表", "排序"],
    },
    {
      title: "验证回文串",
      platform: "leetcode",
      problemId: 125,
      difficulty: "easy",
      tags: ["双指针"],
    },
    {
      title: "字符串中的单词数",
      platform: "leetcode",
      problemId: 434,
      difficulty: "easy",
      tags: ["字符串"],
    },
    {
      title: "同构字符串",
      platform: "leetcode",
      problemId: 205,
      difficulty: "easy",
      tags: ["哈希表", "双射"],
    },
    // 新增 Easy
    {
      title: "二进制求和",
      platform: "leetcode",
      problemId: 67,
      difficulty: "easy",
      tags: ["模拟", "数学"],
    },
    {
      title: "Excel 表列名称",
      platform: "leetcode",
      problemId: 168,
      difficulty: "easy",
      tags: ["数学"],
    },
    {
      title: "Excel 表列序号",
      platform: "leetcode",
      problemId: 171,
      difficulty: "easy",
      tags: ["数学"],
    },
    {
      title: "快乐数",
      platform: "leetcode",
      problemId: 202,
      difficulty: "easy",
      tags: ["哈希集合", "快慢指针"],
    },
    {
      title: "单词规律",
      platform: "leetcode",
      problemId: 290,
      difficulty: "easy",
      tags: ["双射哈希"],
    },
    {
      title: "反转字符串中的元音字母",
      platform: "leetcode",
      problemId: 345,
      difficulty: "easy",
      tags: ["对撞指针"],
    },
    {
      title: "反转字符串中的单词 III",
      platform: "leetcode",
      problemId: 557,
      difficulty: "easy",
      tags: ["双指针"],
    },
    {
      title: "重复的子字符串",
      platform: "leetcode",
      problemId: 459,
      difficulty: "easy",
      tags: ["字符串", "KMP"],
    },
    {
      title: "学生出勤记录 I",
      platform: "leetcode",
      problemId: 551,
      difficulty: "easy",
      tags: ["遍历"],
    },
    {
      title: "检测大写字母",
      platform: "leetcode",
      problemId: 520,
      difficulty: "easy",
      tags: ["字符串"],
    },
    {
      title: "最后一块石头的重量",
      platform: "leetcode",
      problemId: 1046,
      difficulty: "easy",
      tags: ["堆"],
    },
    {
      title: "IP 地址无效化",
      platform: "leetcode",
      problemId: 1108,
      difficulty: "easy",
      tags: ["字符串"],
    },
    {
      title: "解码字母到整数映射",
      platform: "leetcode",
      problemId: 1309,
      difficulty: "easy",
      tags: ["字符串"],
    },
    {
      title: "整理字符串",
      platform: "leetcode",
      problemId: 1544,
      difficulty: "easy",
      tags: ["栈"],
    },
    {
      title: "生成每种字符都是奇数个的字符串",
      platform: "leetcode",
      problemId: 1374,
      difficulty: "easy",
      tags: ["字符串"],
    },
    {
      title: "验证外星语词典",
      platform: "leetcode",
      problemId: 953,
      difficulty: "easy",
      tags: ["哈希表", "排序"],
    },
    {
      title: "增减字符串匹配",
      platform: "leetcode",
      problemId: 942,
      difficulty: "easy",
      tags: ["贪心"],
    },
    {
      title: "唯一摩尔密码",
      platform: "leetcode",
      problemId: 804,
      difficulty: "easy",
      tags: ["哈希集合"],
    },
    {
      title: "山羊拉丁文",
      platform: "leetcode",
      problemId: 824,
      difficulty: "easy",
      tags: ["字符串"],
    },
    {
      title: "比较含退格的字符串",
      platform: "leetcode",
      problemId: 844,
      difficulty: "easy",
      tags: ["栈", "双指针"],
    },
    {
      title: "重新格式化电话号码",
      platform: "leetcode",
      problemId: 1694,
      difficulty: "easy",
      tags: ["字符串"],
    },
    {
      title: "检查二进制字符串字段",
      platform: "leetcode",
      problemId: 1784,
      difficulty: "easy",
      tags: ["字符串"],
    },
    // Medium
    {
      title: "无重复字符的最长子串",
      platform: "leetcode",
      problemId: 3,
      difficulty: "medium",
      tags: ["滑动窗口", "哈希表"],
    },
    {
      title: "字母异位词分组",
      platform: "leetcode",
      problemId: 49,
      difficulty: "medium",
      tags: ["哈希表", "排序"],
    },
    {
      title: "最长回文子串",
      platform: "leetcode",
      problemId: 5,
      difficulty: "medium",
      tags: ["中心扩展", "DP"],
    },
    {
      title: "字符串的排列",
      platform: "leetcode",
      problemId: 567,
      difficulty: "medium",
      tags: ["滑动窗口", "哈希表"],
    },
    {
      title: "单词拆分",
      platform: "leetcode",
      problemId: 139,
      difficulty: "medium",
      tags: ["DP", "哈希表"],
    },
    {
      title: "字符串解码",
      platform: "leetcode",
      problemId: 394,
      difficulty: "medium",
      tags: ["栈", "递归"],
    },
    // 新增 Medium
    {
      title: "最长回文子串",
      platform: "leetcode",
      problemId: 5,
      difficulty: "medium",
      tags: ["中心扩展", "DP"],
    },
    {
      title: "Z 字形变换",
      platform: "leetcode",
      problemId: 6,
      difficulty: "medium",
      tags: ["模拟"],
    },
    {
      title: "字符串转换整数 (atoi)",
      platform: "leetcode",
      problemId: 8,
      difficulty: "medium",
      tags: ["模拟", "边界"],
    },
    {
      title: "电话号码的字母组合",
      platform: "leetcode",
      problemId: 17,
      difficulty: "medium",
      tags: ["回溯", "映射"],
    },
    {
      title: "括号生成",
      platform: "leetcode",
      problemId: 22,
      difficulty: "medium",
      tags: ["回溯", "剪枝"],
    },
    {
      title: "实现 strStr()",
      platform: "leetcode",
      problemId: 28,
      difficulty: "medium",
      tags: ["KMP", "字符串"],
    },
    {
      title: "字母异位词分组",
      platform: "leetcode",
      problemId: 49,
      difficulty: "medium",
      tags: ["哈希表", "排序"],
    },
    {
      title: "简化路径",
      platform: "leetcode",
      problemId: 71,
      difficulty: "medium",
      tags: ["栈"],
    },
    {
      title: "复原 IP 地址",
      platform: "leetcode",
      problemId: 93,
      difficulty: "medium",
      tags: ["回溯", "字符串"],
    },
    {
      title: "最长回文子串",
      platform: "leetcode",
      problemId: 5,
      difficulty: "medium",
      tags: ["中心扩展", "DP"],
    },
    {
      title: "翻转字符串里的单词",
      platform: "leetcode",
      problemId: 151,
      difficulty: "medium",
      tags: ["双指针"],
    },
    {
      title: "比较版本号",
      platform: "leetcode",
      problemId: 165,
      difficulty: "medium",
      tags: ["字符串", "双指针"],
    },
    {
      title: "重复的DNA序列",
      platform: "leetcode",
      problemId: 187,
      difficulty: "medium",
      tags: ["哈希表", "滑动窗口"],
    },
    {
      title: "基本计算器 II",
      platform: "leetcode",
      problemId: 227,
      difficulty: "medium",
      tags: ["栈", "数学"],
    },
    {
      title: "汇总区间",
      platform: "leetcode",
      problemId: 228,
      difficulty: "medium",
      tags: ["遍历"],
    },
    {
      title: "找到字符串中所有字母异位词",
      platform: "leetcode",
      problemId: 438,
      difficulty: "medium",
      tags: ["定长窗口", "哈希表"],
    },
    {
      title: "字符串的排列",
      platform: "leetcode",
      problemId: 567,
      difficulty: "medium",
      tags: ["定长窗口", "哈希表"],
    },
    {
      title: "最长回文子序列",
      platform: "leetcode",
      problemId: 516,
      difficulty: "medium",
      tags: ["DP", "区间"],
    },
    {
      title: "最长重复子串",
      platform: "leetcode",
      problemId: 1044,
      difficulty: "medium",
      tags: ["二分", "哈希"],
    },
  ],
});

// ================================================================
// 生成完整文件
// ================================================================

// 对每个分类的 items 排序（Easy 在前，Medium 在后），标记重复
phase1.forEach(function (cat) {
  cat.items.sort(byDiff);
  cat.items.forEach(maybeRepeat);
  // 去除同分类内的重复题
  var seen = {};
  cat.items = cat.items.filter(function (item) {
    var key = item.platform + ":" + item.problemId;
    if (seen[key]) return false;
    seen[key] = true;
    return true;
  });
});

// 输出到文件
var output = '"use strict";\n' + "\n" + "const PRACTICE_CATEGORIES = [\n";

phase1.forEach(function (cat, ci) {
  if (ci > 0) output += "\n";
  output += "  // ── " + (ci + 1) + "." + cat.order + " " + cat.title + " ──\n";
  output += "  {\n";
  output += '    id: "' + cat.id + '",\n';
  output += '    title: "' + cat.title + '",\n';
  output += "    phase: " + cat.phase + ",\n";
  output += "    order: " + cat.order + ",\n";
  output += '    icon: "' + cat.icon + '",\n';
  output += '    desc: "' + cat.desc + '",\n';
  output += "    items: [\n";

  cat.items.forEach(function (item, ii) {
    if (ii > 0) output += "\n";
    output += "      {\n";
    output += '        title: "' + item.title + '",\n';
    output += '        platform: "' + item.platform + '",\n';
    output +=
      "        problemId: " +
      (typeof item.problemId === "string"
        ? '"' + item.problemId + '"'
        : item.problemId) +
      ",\n";
    output += '        difficulty: "' + item.difficulty + '",\n';
    output += "        tags: " + JSON.stringify(item.tags) + ",\n";
    if (item.repeat) output += "        repeat: true,\n";
    output += "      },\n";
  });

  output += "    ],\n";
  output += "  },\n";
});

output += "];\n";

var outPath = path.resolve(__dirname, "..", "data", "app", "practice-data.js");
fs.writeFileSync(outPath, output, "utf-8");
console.log("Written to " + outPath);
console.log("Categories: " + phase1.length);
var total = phase1.reduce(function (s, c) {
  return s + c.items.length;
}, 0);
console.log("Total problems: " + total);
