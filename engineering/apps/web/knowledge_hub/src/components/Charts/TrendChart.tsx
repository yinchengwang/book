/**
 * 成长趋势图组件
 * 使用 Recharts 展示 14 天学习趋势
 */
import { Component } from 'react'
import { View, Text } from '@tarojs/components'
import { BarChart, Bar, XAxis, YAxis, Tooltip, ResponsiveContainer, Cell } from 'recharts'
import { useDailyDataStore } from '@/stores/dailyDataStore'
import './Charts.scss'

interface TrendChartProps {
  /** 天数，默认 14 */
  days?: number
  /** 高度 */
  height?: number
}

// 趋势图组件
class TrendChart extends Component<TrendChartProps> {
  state = {
    data: [] as { date: string; score: number; label: string }[],
    isEmpty: false,
  }

  componentDidMount() {
    this.loadData()
  }

  loadData = () => {
    const { days = 14 } = this.props
    const snapshots = useDailyDataStore.getState().getRecentSnapshots(days)

    // 生成最近 N 天的数据
    const result: { date: string; score: number; label: string }[] = []
    for (let i = days - 1; i >= 0; i--) {
      const d = new Date()
      d.setDate(d.getDate() - i)
      const dateStr = d.toISOString().slice(0, 10)
      const snapshot = snapshots.find(s => s.date === dateStr)

      result.push({
        date: dateStr,
        score: snapshot?.totalScore || 0,
        label: `${d.getMonth() + 1}/${d.getDate()}`,
      })
    }

    // 检查是否有数据
    const hasData = result.some(d => d.score > 0)
    this.setState({ data: result, isEmpty: !hasData })
  }

  // 渲染空状态
  renderEmpty = () => (
    <View className="chart-empty">
      <Text className="empty-icon">📊</Text>
      <Text className="empty-text">暂无趋势数据</Text>
      <Text className="empty-hint">完成学习后将自动记录趋势</Text>
    </View>
  )

  render() {
    const { height = 200 } = this.props
    const { data, isEmpty } = this.state

    if (isEmpty) {
      return (
        <View className="trend-chart-container">
          <Text className="section-title">📈 成长趋势</Text>
          {this.renderEmpty()}
        </View>
      )
    }

    // 获取颜色渐变
    const getBarColor = (value: number) => {
      if (value >= 80) return '#10b981' // 绿色 - 优秀
      if (value >= 60) return '#6366f1' // 紫色 - 良好
      if (value >= 40) return '#f59e0b' // 橙色 - 一般
      return '#94a3b8' // 灰色 - 需努力
    }

    return (
      <View className="trend-chart-container">
        <Text className="section-title">📈 成长趋势</Text>
        <View className="chart-wrapper" style={{ height: `${height}px` }}>
          {/* #ifdef H5 */}
          <ResponsiveContainer width="100%" height="100%">
            <BarChart data={data} margin={{ top: 10, right: 10, left: -20, bottom: 0 }}>
              <XAxis
                dataKey="label"
                tick={{ fill: '#94a3b8', fontSize: 10 }}
                axisLine={{ stroke: 'rgba(255,255,255,0.1)' }}
                tickLine={false}
              />
              <YAxis
                domain={[0, 100]}
                tick={{ fill: '#94a3b8', fontSize: 10 }}
                axisLine={false}
                tickLine={false}
              />
              <Tooltip
                contentStyle={{
                  background: '#1e293b',
                  border: '1px solid rgba(255,255,255,0.1)',
                  borderRadius: '8px',
                  color: '#fff',
                }}
                formatter={(value: number) => [`${value}%`, '掌握度']}
                labelFormatter={(label) => `日期: ${label}`}
              />
              <Bar dataKey="score" radius={[4, 4, 0, 0]}>
                {data.map((entry, index) => (
                  <Cell key={`cell-${index}`} fill={getBarColor(entry.score)} />
                ))}
              </Bar>
            </BarChart>
          </ResponsiveContainer>
          {/* #endif */}

          {/* #ifdef MP-WEIXIN */}
          <View className="mp-chart-placeholder">
            {data.map((item, index) => (
              <View key={index} className="bar-item">
                <View
                  className="bar-fill"
                  style={{
                    height: `${item.score}%`,
                    backgroundColor: getBarColor(item.score),
                  }}
                />
                <Text className="bar-label">{item.label}</Text>
              </View>
            ))}
          </View>
          {/* #endif */}
        </View>
      </View>
    )
  }
}

export default TrendChart
