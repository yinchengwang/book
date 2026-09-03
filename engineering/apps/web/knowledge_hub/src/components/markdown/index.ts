/**
 * Markdown 渲染组件 - 平台适配导出
 *
 * H5 端使用 react-markdown 版本
 * 小程序端使用自定义解析器版本
 */

// #ifdef H5
export { MarkdownRenderer as MarkdownRenderer } from './MarkdownRenderer'
// #endif

// #ifdef MP-WEIXIN
export { MarkdownRendererWeapp as MarkdownRenderer } from './MarkdownRenderer.weapp'
// #endif

// 默认导出 - 根据环境自动选择
// #ifdef H5
import { MarkdownRenderer as DefaultMarkdownRenderer } from './MarkdownRenderer'
// #endif
// #ifdef MP-WEIXIN
import { MarkdownRendererWeapp as DefaultMarkdownRenderer } from './MarkdownRenderer.weapp'
// #endif

export default DefaultMarkdownRenderer
