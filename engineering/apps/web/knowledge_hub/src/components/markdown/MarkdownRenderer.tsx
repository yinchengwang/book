/**
 * H5 端 Markdown 渲染组件
 *
 * 使用 react-markdown + remark-gfm + react-syntax-highlighter
 * 支持：标题、代码块（语法高亮）、图片、表格、引用、列表、链接
 */
import ReactMarkdown from 'react-markdown'
import remarkGfm from 'remark-gfm'
import { Prism as SyntaxHighlighter } from 'react-syntax-highlighter'
import { vscDarkPlus } from 'react-syntax-highlighter/dist/esm/styles/prism'
import type { Components } from 'react-markdown'
import './MarkdownRenderer.scss'

// 组件 Props
interface MarkdownRendererProps {
  content: string
  className?: string
}

// 代码块组件
const CodeBlock = ({ children, className }: { children: string; className?: string }) => {
  const match = /language-(\w+)/.exec(className || '')
  const language = match ? match[1] : 'text'

  return (
    <div className="md-code-block">
      {language !== 'text' && (
        <span className="code-language">{language}</span>
      )}
      <SyntaxHighlighter
        style={vscDarkPlus}
        language={language}
        PreTag="div"
        customStyle={{
          margin: 0,
          borderRadius: '0 0 8px 8px',
          fontSize: '13px',
        }}
      >
        {String(children).replace(/\n$/, '')}
      </SyntaxHighlighter>
    </div>
  )
}

// 图片组件
const Image = ({ src, alt }: { src?: string; alt?: string }) => {
  if (!src) return null

  return (
    <figure className="md-image-figure">
      <img src={src} alt={alt || ''} className="md-image" loading="lazy" />
      {alt && <figcaption className="md-image-caption">{alt}</figcaption>}
    </figure>
  )
}

// 表格组件
const Table = ({ children }: { children: React.ReactNode }) => {
  return (
    <div className="md-table-wrapper">
      <table className="md-table">{children}</table>
    </div>
  )
}

// Markdown 渲染组件
export const MarkdownRenderer = ({ content, className = '' }: MarkdownRendererProps) => {
  if (!content || !content.trim()) {
    return <div className="md-empty">暂无内容</div>
  }

  const components: Components = {
    // 代码块
    code({ node, className, children, ...props }) {
      const match = /language-(\w+)/.exec(className || '')
      const isInline = !match && !className

      if (isInline) {
        return (
          <code className="md-inline-code" {...props}>
            {children}
          </code>
        )
      }

      return <CodeBlock className={className}>{String(children)}</CodeBlock>
    },

    // 图片
    img({ src, alt }) {
      return <Image src={src} alt={alt} />
    },

    // 表格
    table({ children }) {
      return <Table>{children}</Table>
    },

    // 链接 - 外部链接新窗口打开
    a({ href, children }) {
      const isExternal = href?.startsWith('http')
      return (
        <a
          href={href}
          target={isExternal ? '_blank' : undefined}
          rel={isExternal ? 'noopener noreferrer' : undefined}
          className="md-link"
        >
          {children}
        </a>
      )
    },
  }

  return (
    <div className={`md-content ${className}`}>
      <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        components={components}
      >
        {content}
      </ReactMarkdown>
    </div>
  )
}

export default MarkdownRenderer
