/**
 * 图解系列搜索栏组件
 */
import { View, Input, Text } from '@tarojs/components'
import { useState } from 'react'
import './learn_deep.scss'

interface SearchBarProps {
  value: string
  onChange: (value: string) => void
}

export const SearchBar = ({ value, onChange }: SearchBarProps) => {
  return (
    <View className="search-bar">
      <Text className="search-icon">🔍</Text>
      <Input
        type="text"
        placeholder="搜索文章..."
        value={value}
        onInput={(e: any) => onChange(e.detail.value)}
        className="search-input"
      />
      {value && (
        <Text className="search-clear" onClick={() => onChange('')}>✕</Text>
      )}
    </View>
  )
}