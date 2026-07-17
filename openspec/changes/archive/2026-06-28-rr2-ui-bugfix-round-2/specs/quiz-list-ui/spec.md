# quiz-list-ui

## Purpose

将 Quiz 题库浏览（list 模式）从单行紧凑卡片改为折叠式卡片，默认简洁、点击展开查看题目详情，提升信息密度与可操作性。

## Requirements

### Requirement: 折叠式卡片

Quiz list 模式下的每道题卡片必须支持折叠/展开两种状态：

- **默认折叠**：仅显示 title + 难度徽章 + 分类徽章 + domain 徽章 + 已答次数
- **展开后**：在前者基础上增加 description 前 100 字 + options 预览（选择题）+ 收藏按钮 + "直接答此题"按钮

#### Scenario: 默认折叠
- **WHEN** 用户进入 Quiz 题库浏览（list 模式）且未点击任何卡片
- **THEN** 所有卡片显示单行视图（title + 3 徽章 + 已答次数）
- **AND** 列表高度合理，单屏可见 8-12 张卡片

#### Scenario: 点击展开
- **WHEN** 用户点击单张卡片
- **THEN** 卡片展开，显示 description 预览、options 预览（若有）、收藏按钮、"直接答此题"按钮
- **AND** 展开图标从 ▶ 变为 ▼

#### Scenario: 再次点击折叠
- **WHEN** 展开态用户再次点击卡片
- **THEN** 卡片回到单行视图
- **AND** 展开状态可记忆（可选）：localStorage 保存展开 ID

### Requirement: 列表顶部统计

list 模式顶部必须显示 3 项统计：当前 stack 题数、已答数、答对数：

```tsx
<View className="list-stats">
  <View className="list-stat">
    <Text className="stat-num">{totalForStack}</Text>
    <Text className="stat-label">{stackName} 题数</Text>
  </View>
  <View className="list-stat">
    <Text className="stat-num">{answeredCount}</Text>
    <Text className="stat-label">已答</Text>
  </View>
  <View className="list-stat">
    <Text className="stat-num">{correctCount}</Text>
    <Text className="stat-label">答对</Text>
  </View>
</View>
```

#### Scenario: stack 切换更新统计
- **WHEN** 用户在 stack filter 切换到 CPP
- **THEN** 顶部统计 3 项数字更新为 CPP stack 的真实数据

### Requirement: 折叠卡样式隔离

新增 `.question-list-item--expanded` BEM 修饰类，与现有 `.question-list-item` 隔离：

```scss
.question-list-item {
  // 单行视图样式
  &--expanded {
    // 展开后样式
  }
}
```

#### Scenario: 样式不污染其他模式
- **WHEN** 用户在 list 模式展开卡片
- **THEN** plan / quiz / wrong / stats 模式不受影响
- **AND** dark / light 双主题均正常