/**
 * 小程序端 Markdown 解析器
 *
 * 使用 taroify 的 Markdown 组件或自定义实现
 * 兼容 Taro 环境
 */
import { View, Text, Image, ScrollView } from '@tarojs/components'
import Taro from '@tarojs/taro'
import './MarkdownRenderer.weapp.scss'

interface MarkdownRendererProps {
  content: string
  className?: string
}

// 解析 Markdown 为 AST
interface MarkdownNode {
  type: 'heading' | 'paragraph' | 'code' | 'image' | 'blockquote' | 'list' | 'table' | 'hr' | 'text'
  level?: number  // heading 级别
  content?: string
  language?: string  // code language
  src?: string  // image src
  alt?: string  // image alt
  items?: string[]  // list items
  rows?: string[][]  // table rows
  children?: MarkdownNode[]
}

// 简单 Markdown 解析器
function parseMarkdown(md: string): MarkdownNode[] {
  const lines = md.split('\n')
  const nodes: MarkdownNode[] = []
  let i = 0

  while (i < lines.length) {
    const line = lines[i]
    const trimmed = line.trim()

    // 空行
    if (!trimmed) {
      i++
      continue
    }

    // 标题
    if (trimmed.startsWith('#')) {
      const match = trimmed.match(/^(#{1,6})\s+(.+)/)
      if (match) {
        nodes.push({
          type: 'heading',
          level: match[1].length,
          content: match[2]
        })
        i++
        continue
      }
    }

    // 代码块
    if (trimmed.startsWith('```')) {
      const codeLines: string[] = []
      const lang = trimmed.slice(3).trim() || 'text'
      i++
      while (i < lines.length && !lines[i].trim().startsWith('```')) {
        codeLines.push(lines[i])
        i++
      }
      nodes.push({
        type: 'code',
        language: lang,
        content: codeLines.join('\n')
      })
      i++  // 跳过结束的 ```
      continue
    }

    // 水平线
    if (/^[-*_]{3,}$/.test(trimmed)) {
      nodes.push({ type: 'hr' })
      i++
      continue
    }

    // 引用
    if (trimmed.startsWith('>')) {
      const quoteLines: string[] = []
      while (i < lines.length && lines[i].trim().startsWith('>')) {
        quoteLines.push(lines[i].trim().slice(1).trim())
        i++
      }
      nodes.push({
        type: 'blockquote',
        content: quoteLines.join('\n')
      })
      continue
    }

    // 列表
    if (/^[-*]\s/.test(trimmed) || /^\d+\.\s/.test(trimmed)) {
      const items: string[] = []
      while (i < lines.length && /^[-*]\s/.test(lines[i].trim())) {
        items.push(lines[i].trim().replace(/^[-*]\s/, ''))
        i++
      }
      if (items.length > 0) {
        nodes.push({ type: 'list', items })
      }
      continue
    }

    // 图片
    const imgMatch = trimmed.match(/!\[([^\]]*)\]\(([^)]+)\)/)
    if (imgMatch) {
      nodes.push({
        type: 'image',
        alt: imgMatch[1],
        src: imgMatch[2]
      })
      i++
      continue
    }

    // 段落
    nodes.push({
      type: 'paragraph',
      content: trimmed
    })
    i++
  }

  return nodes
}

// 渲染标题
const renderHeading = (node: MarkdownNode, index: number) => {
  const level = node.level || 1
  const sizeMap = { 1: 24, 2: 20, 3: 18, 4: 16, 5: 15, 6: 14 }
  const size = sizeMap[level as keyof typeof sizeMap] || 16

  return (
    <Text
      key={index}
      className={`md-h${level}`}
      style={{ fontSize: `${size}px`, fontWeight: 600 }}
    >
      {node.content}
    </Text>
  )
}

// 渲染代码块
const renderCode = (node: MarkdownNode, index: number) => {
  return (
    <ScrollView
      key={index}
      className="md-code-block"
      scrollX
      enhanced
      showHorizontalScrollbar={false}
    >
      <Text className="md-code-lang">{node.language}</Text>
      <Text className="md-code-text">{node.content}</Text>
    </ScrollView>
  )
}

// 渲染图片
const renderImage = (node: MarkdownNode, index: number) => {
  return (
    <View key={index} className="md-image-figure">
      <Image
        className="md-image"
        src={node.src}
        mode="widthFix"
        mode="aspectFit"
        lazyLoad
      />
      {node.alt && (
        <Text className="md-image-caption">{node.alt}</Text>
      )}
    </View>
  )
}

// 渲染引用
const renderBlockquote = (node: MarkdownNode, index: number) => {
  return (
    <View key={index} className="md-blockquote">
      <Text className="md-blockquote-text">{node.content}</Text>
    </View>
  )
}

// 渲染列表
const renderList = (node: MarkdownNode, index: number) => {
  return (
    <View key={index} className="md-list">
      {node.items?.map((item, i) => (
        <View key={i} className="md-list-item">
          <Text className="md-list-bullet">•</Text>
          <Text className="md-list-text">{item}</Text>
        </View>
      ))}
    </View>
  )
}

// 渲染段落（支持简单的格式）
const renderParagraph = (node: MarkdownNode, index: number) => {
  let content = node.content || ''

  // 处理粗体 **text**
  const parts: Array<{ text: string; bold?: boolean; code?: boolean }> = []
  let remaining = content
  let key = 0

  while (remaining) {
    // 代码 `code`
    const codeMatch = remaining.match(/^`([^`]+)`/)
    if (codeMatch) {
      parts.push({ text: codeMatch[1], code: true })
      remaining = remaining.slice(codeMatch[0].length)
      continue
    }

    // 粗体 **text**
    const boldMatch = remaining.match(/^\*\*([^*]+)\*\*/)
    if (boldMatch) {
      parts.push({ text: boldMatch[1], bold: true })
      remaining = remaining.slice(boldMatch[0].length)
      continue
    }

    // 普通文字
    const normalMatch = remaining.match(/^[^*`]+/)
    if (normalMatch) {
      parts.push({ text: normalMatch[0] })
      remaining = remaining.slice(normalMatch[0].length)
      continue
    }

    // 单个字符
    parts.push({ text: remaining[0] })
    remaining = remaining.slice(1)
  }

  return (
    <View key={index} className="md-paragraph">
      {parts.map((part, i) => (
        <Text
          key={i}
          className={part.code ? 'md-inline-code' : ''}
          style={part.bold ? { fontWeight: 600 } : undefined}
        >
          {part.text}
        </Text>
      ))}
    </View>
  )
}

// 主组件
export const MarkdownRendererWeapp = ({ content, className = '' }: MarkdownRendererProps) => {
  if (!content || !content.trim()) {
    return (
      <View className={`md-empty ${className}`}>
        <Text>暂无内容</Text>
      </View>
    )
  }

  const nodes = parseMarkdown(content)

  return (
    <View className={`md-content ${className}`}>
      {nodes.map((node, index) => {
        switch (node.type) {
          case 'heading':
            return renderHeading(node, index)
          case 'code':
            return renderCode(node, index)
          case 'image':
            return renderImage(node, index)
          case 'blockquote':
            return renderBlockquote(node, index)
          case 'list':
            return renderList(node, index)
          case 'hr':
            return <View key={index} className="md-hr" />
          case 'paragraph':
            return renderParagraph(node, index)
          default:
            return null
        }
      })}
    </View>
  )
}

export default MarkdownRendererWeapp
