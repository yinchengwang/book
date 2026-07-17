import { HTMLAttributes, ReactNode, forwardRef } from 'react'

/**
 * Taro <Text> 适配 H5 → <span>
 * Taro 中 <Text> 是行内元素，<View> 是块级元素
 * H5 端用 span/inline-block 区分
 */
export interface TextProps extends HTMLAttributes<HTMLSpanElement> {
  selectable?: boolean
  space?: 'nbsp' | 'ensp' | 'emsp'
  decode?: boolean
  userSelect?: 'text' | 'none' | 'all'
  children?: ReactNode
}

export const Text = forwardRef<HTMLSpanElement, TextProps>((props, ref) => {
  const {
    selectable,
    space,
    decode,
    userSelect = selectable ? 'text' : 'none',
    style,
    children,
    ...rest
  } = props

  // 处理 space 属性
  let processedChildren = children
  if (space && typeof children === 'string') {
    const spaceMap = {
      nbsp: ' ',
      ensp: ' ',
      emsp: ' '
    }
    const replacement = spaceMap[space]
    processedChildren = children.split(' ').join(replacement)
  }

  return (
    <span
      ref={ref}
      style={{
        userSelect,
        ...style
      }}
      {...rest}
    >
      {processedChildren}
    </span>
  )
})

Text.displayName = 'Text'
