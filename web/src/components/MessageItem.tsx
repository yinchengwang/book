import { useState } from 'react'
import ReactMarkdown from 'react-markdown'
import { Prism as SyntaxHighlighter } from 'react-syntax-highlighter'
import { vscDarkPlus } from 'react-syntax-highlighter/dist/esm/styles/prism'
import type { Message, ChunkReference } from '@/types'
import { cn } from '@/lib/utils'
import { FileText, ChevronDown, ChevronUp, Copy, Check } from 'lucide-react'

interface MessageItemProps {
  message: Message
  onChunkClick?: (chunk: ChunkReference) => void
}

export const MessageItem = ({ message, onChunkClick }: MessageItemProps) => {
  const [expandedChunks, setExpandedChunks] = useState(false)
  const [copied, setCopied] = useState(false)

  const isUser = message.role === 'user'

  const handleCopy = async () => {
    await navigator.clipboard.writeText(message.content)
    setCopied(true)
    setTimeout(() => setCopied(false), 2000)
  }

  return (
    <div className={cn('flex gap-3 p-4', isUser ? 'justify-end' : 'justify-start')}>
      <div className={cn(
        'max-w-[85%] md:max-w-[75%] rounded-lg px-4 py-2',
        isUser ? 'bg-blue-600 text-white' : 'bg-gray-100 dark:bg-gray-800'
      )}>
        <div className={cn(
          'text-xs mb-1',
          isUser ? 'text-blue-200' : 'text-gray-500 dark:text-gray-400'
        )}>
          {isUser ? '用户' : 'AI 助手'}
        </div>

        <div className="prose prose-sm dark:prose-invert max-w-none break-words">
          <ReactMarkdown
            components={{
              code({ className, children, ...props }) {
                const match = /language-(\w+)/.exec(className || '')
                return match ? (
                  <SyntaxHighlighter
                    style={vscDarkPlus}
                    language={match[1]}
                    PreTag="div"
                  >
                    {String(children).replace(/\n$/, '')}
                  </SyntaxHighlighter>
                ) : (
                  <code className={className} {...props}>
                    {children}
                  </code>
                )
              }
            }}
          >
            {message.content}
          </ReactMarkdown>
        </div>

        {message.chunks && message.chunks.length > 0 && (
          <div className="mt-3 pt-3 border-t border-gray-200 dark:border-gray-700">
            <button
              onClick={() => setExpandedChunks(!expandedChunks)}
              className="flex items-center gap-1 text-sm text-gray-600 hover:text-gray-900 dark:text-gray-400 dark:hover:text-gray-100"
            >
              <FileText className="w-4 h-4" />
              引用文档 ({message.chunks.length})
              {expandedChunks ? <ChevronUp className="w-4 h-4" /> : <ChevronDown className="w-4 h-4" />}
            </button>

            {expandedChunks && (
              <div className="mt-2 space-y-2">
                {message.chunks.map((chunk) => (
                  <button
                    key={chunk.id}
                    onClick={() => onChunkClick?.(chunk)}
                    className="w-full text-left p-2 rounded bg-gray-50 hover:bg-gray-100 text-sm dark:bg-gray-900 dark:hover:bg-gray-700"
                  >
                    <div className="flex items-center gap-2">
                      <FileText className="w-4 h-4 text-gray-400 shrink-0" />
                      <span className="truncate">{chunk.file_path}</span>
                    </div>
                    <div className="text-xs text-gray-500 mt-1">
                      Score: {chunk.score.toFixed(2)}
                    </div>
                  </button>
                ))}
              </div>
            )}
          </div>
        )}

        {!isUser && (
          <div className="mt-2 flex gap-2">
            <button
              onClick={handleCopy}
              className="text-xs text-gray-500 hover:text-gray-700 flex items-center gap-1 dark:hover:text-gray-300"
            >
              {copied ? <Check className="w-3 h-3" /> : <Copy className="w-3 h-3" />}
              {copied ? '已复制' : '复制'}
            </button>
          </div>
        )}

        <div className={cn('text-xs mt-1', isUser ? 'text-blue-200' : 'text-gray-400')}>
          {new Date(message.timestamp).toLocaleTimeString('zh-CN')}
        </div>
      </div>
    </div>
  )
}