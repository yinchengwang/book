/**
 * Taro 组件适配层统一导出
 *
 * 这个文件被 Vite alias 解析：
 * import { View, Text, Button } from '@tarojs/components'
 * ↓
 * import { View, Text, Button } from './adapters/index.ts'
 *
 * 源码完全不需要修改
 */

// 基础组件
export { View } from './View'
export type { ViewProps } from './View'

export { Text } from './Text'
export type { TextProps } from './Text'

export { Button } from './Button'
export type { ButtonProps, ButtonType, ButtonSize } from './Button'

export { Image } from './Image'
export type { ImageProps, ImageMode } from './Image'

export { Input } from './Input'
export type { InputProps, InputType } from './Input'

export { Textarea } from './Textarea'
export type { TextareaProps } from './Textarea'

export { ScrollView } from './ScrollView'
export type { ScrollViewProps } from './ScrollView'

export { Switch } from './Switch'
export type { SwitchProps } from './Switch'

export { Progress } from './Progress'
export type { ProgressProps } from './Progress'

// 默认导出 - 兼容 `import TaroComp from '@tarojs/components'`
import { View } from './View'
import { Text } from './Text'
import { Button } from './Button'
import { Image } from './Image'
import { Input } from './Input'
import { Textarea } from './Textarea'
import { ScrollView } from './ScrollView'
import { Switch } from './Switch'
import { Progress } from './Progress'

export default {
  View,
  Text,
  Button,
  Image,
  Input,
  Textarea,
  ScrollView,
  Switch,
  Progress
}
