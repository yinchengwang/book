/**
 * H5 端 TabBar 占位 - 替换小程序版的 TabBar
 * PC 端用 Sidebar，移动端不需要底部 TabBar
 * 这里返回 null 占位
 */
import { Component } from 'react'

// H5 端不渲染（用 Sidebar 替代）
const TabBar: React.FC<any> = () => null
export default TabBar
