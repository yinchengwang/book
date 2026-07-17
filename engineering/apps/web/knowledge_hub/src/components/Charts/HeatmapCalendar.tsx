/**
 * 热力图日历组件
 * GitHub 风格展示每日学习活动
 */
import { Component } from 'react'
import { View, Text } from '@tarojs/components'
import { useDailyDataStore } from '@/stores/dailyDataStore'
import './Charts.scss'

interface HeatmapCalendarProps {
  /** 展示周数，默认 52 周（一年） */
  weeks?: number
  /** 单元格大小 */
  cellSize?: number
  /** 单元格间距 */
  gap?: number
}

// 颜色等级
const LEVELS = [
  { max: 0, color: '#161b22', label: '无活动' },
  { max: 5, color: '#0e4429', label: '1-5 题' },
  { max: 10, color: '#006d32', label: '6-10 题' },
  { max: 20, color: '#26a641', label: '11-20 题' },
  { max: Infinity, color: '#39d353', label: '20+ 题' },
]

// 获取颜色等级
const getLevelColor = (count: number): string => {
  for (const level of LEVELS) {
    if (count <= level.max) return level.color
  }
  return LEVELS[LEVELS.length - 1].color
}

// 月份标签
const MONTHS = ['1月', '2月', '3月', '4月', '5月', '6月', '7月', '8月', '9月', '10月', '11月', '12月']

class HeatmapCalendar extends Component<HeatmapCalendarProps> {
  state = {
    data: [] as { date: string; count: number }[][],
    monthLabels: [] as { month: number; weekIndex: number }[],
  }

  componentDidMount() {
    this.generateCalendar()
  }

  generateCalendar = () => {
    const { weeks = 52, cellSize = 12, gap = 3 } = this.props
    const heatmapData = useDailyDataStore.getState().getHeatmapData()

    // 计算起始日期（从一年前的周日开始）
    const today = new Date()
    const startDate = new Date(today)
    startDate.setDate(startDate.getDate() - weeks * 7)
    // 调整到周日
    startDate.setDate(startDate.getDate() - startDate.getDay())

    const result: { date: string; count: number }[][] = []
    const months: { month: number; weekIndex: number }[] = []
    let currentDate = new Date(startDate)
    let currentMonth = -1

    for (let w = 0; w < weeks; w++) {
      const week: { date: string; count: number }[] = []

      for (let d = 0; d < 7; d++) {
        const dateStr = currentDate.toISOString().slice(0, 10)
        const count = heatmapData[dateStr] || 0

        week.push({ date: dateStr, count })

        // 记录月份变化
        const month = currentDate.getMonth()
        if (month !== currentMonth && d === 0) {
          months.push({ month, weekIndex: w })
          currentMonth = month
        }

        currentDate.setDate(currentDate.getDate() + 1)
      }

      result.push(week)
    }

    this.setState({ data: result, monthLabels: months })
  }

  // 渲染单个格子
  renderCell = (item: { date: string; count: number }, index: number) => {
    const { cellSize = 12, gap = 3 } = this.props

    return (
      <View
        key={index}
        className="heatmap-cell"
        style={{
          width: `${cellSize}px`,
          height: `${cellSize}px`,
          backgroundColor: getLevelColor(item.count),
          marginRight: `${gap}px`,
          marginBottom: `${gap}px`,
        }}
      >
        <Text className="cell-tooltip">
          {item.date}: {item.count} 题
        </Text>
      </View>
    )
  }

  // 渲染星期标签
  renderWeekLabels = () => {
    const labels = ['', '一', '', '三', '', '五', '']
    return (
      <View className="week-labels">
        {labels.map((label, index) => (
          <Text key={index} className="week-label">
            {label}
          </Text>
        ))}
      </View>
    )
  }

  // 渲染月份标签
  renderMonthLabels = () => {
    const { monthLabels } = this.state
    const { cellSize = 12, gap = 3 } = this.props

    return (
      <View className="month-labels">
        {monthLabels.map((item, index) => (
          <Text
            key={index}
            className="month-label"
            style={{ left: `${item.weekIndex * (cellSize + gap)}px` }}
          >
            {MONTHS[item.month]}
          </Text>
        ))}
      </View>
    )
  }

  // 渲染图例
  renderLegend = () => (
    <View className="heatmap-legend">
      <Text className="legend-text">少</Text>
      {LEVELS.map((level, index) => (
        <View
          key={index}
          className="legend-cell"
          style={{ backgroundColor: level.color }}
        />
      ))}
      <Text className="legend-text">多</Text>
    </View>
  )

  render() {
    const { data, monthLabels } = this.state
    const { weeks = 52, cellSize = 12, gap = 3 } = this.props
    const isEmpty = data.length === 0 || data.every(week => week.every(day => day.count === 0))

    if (isEmpty) {
      return (
        <View className="heatmap-container">
          <Text className="section-title">🔥 学习热力图</Text>
          <View className="chart-empty">
            <Text className="empty-icon">📅</Text>
            <Text className="empty-text">暂无学习记录</Text>
            <Text className="empty-hint">开始学习后将自动生成热力图</Text>
          </View>
        </View>
      )
    }

    const totalWidth = weeks * (cellSize + gap)
    const totalHeight = 7 * (cellSize + gap)

    return (
      <View className="heatmap-container">
        <Text className="section-title">🔥 学习热力图</Text>
        <View className="heatmap-wrapper">
          {this.renderWeekLabels()}
          <View className="heatmap-content">
            {this.renderMonthLabels()}
            <View className="heatmap-grid" style={{ width: `${totalWidth}px` }}>
              {data.map((week, weekIndex) => (
                <View key={weekIndex} className="heatmap-week">
                  {week.map((day, dayIndex) => this.renderCell(day, weekIndex * 7 + dayIndex))}
                </View>
              ))}
            </View>
          </View>
        </View>
        {this.renderLegend()}
      </View>
    )
  }
}

export default HeatmapCalendar
