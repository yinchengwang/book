import { Dialog, DialogContent, DialogHeader, DialogTitle } from './ui/dialog'
import { ScrollArea } from './ui/scroll-area'
import { Button } from './ui/button'
import ReactMarkdown from 'react-markdown'
import { Prism as SyntaxHighlighter } from 'react-syntax-highlighter'
import { vscDarkPlus } from 'react-syntax-highlighter/dist/esm/styles/prism'
import { useState } from 'react'
import type { DocumentContent } from '@/types'

interface DocumentPreviewProps {
  doc: DocumentContent
  onClose: () => void
}

export const DocumentPreview = ({ doc, onClose }: DocumentPreviewProps) => {
  const [copied, setCopied] = useState(false)

  const handleCopy = async () => {
    await navigator.clipboard.writeText(doc.file_path)
    setCopied(true)
    setTimeout(() => setCopied(false), 2000)
  }

  return (
    <Dialog open={true} onOpenChange={(open) => { if (!open) onClose() }}>
      <DialogContent className="max-w-4xl w-[95vw] max-h-[85vh] flex flex-col">
        <DialogHeader className="shrink-0">
          <DialogTitle className="truncate pr-8">{doc.title || doc.file_path}</DialogTitle>
          <div className="text-xs text-gray-500 truncate">{doc.file_path}</div>
          <div className="flex gap-2 mt-2">
            <Button variant="outline" size="sm" onClick={handleCopy}>
              {copied ? '已复制' : '复制路径'}
            </Button>
          </div>
        </DialogHeader>

        <ScrollArea className="flex-1 mt-2 border-t border-gray-200 pt-4 dark:border-gray-700">
          <div className="prose prose-sm dark:prose-invert max-w-none">
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
              {doc.content}
            </ReactMarkdown>
          </div>
        </ScrollArea>
      </DialogContent>
    </Dialog>
  )
}