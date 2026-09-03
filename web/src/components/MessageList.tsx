import { useEffect, useRef } from 'react'
import type { Message, ChunkReference } from '@/types'
import { MessageItem } from './MessageItem'
import { ScrollArea } from './ui/scroll-area'

interface MessageListProps {
  messages: Message[]
  isLoading: boolean
  onChunkClick?: (chunk: ChunkReference) => void
}

export const MessageList = ({ messages, isLoading, onChunkClick }: MessageListProps) => {
  const bottomRef = useRef<HTMLDivElement>(null)

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [messages, isLoading])

  return (
    <ScrollArea className="flex-1 px-2 md:px-4">
      <div className="py-4 space-y-2 max-w-4xl mx-auto">
        {messages.length === 0 ? (
          <div className="text-center text-gray-500 dark:text-gray-400 py-12">
            <p className="text-lg">开始对话</p>
            <p className="text-sm mt-2">输入你的问题，AI 将基于知识库回答</p>
          </div>
        ) : (
          messages.map((message) => (
            <MessageItem
              key={message.id}
              message={message}
              onChunkClick={onChunkClick}
            />
          ))
        )}

        {isLoading && (
          <div className="flex gap-3 p-4">
            <div className="bg-gray-100 dark:bg-gray-800 rounded-lg px-4 py-3">
              <div className="flex gap-1">
                <span className="w-2 h-2 bg-gray-400 rounded-full animate-bounce" />
                <span className="w-2 h-2 bg-gray-400 rounded-full animate-bounce" style={{ animationDelay: '0.1s' }} />
                <span className="w-2 h-2 bg-gray-400 rounded-full animate-bounce" style={{ animationDelay: '0.2s' }} />
              </div>
            </div>
          </div>
        )}

        <div ref={bottomRef} />
      </div>
    </ScrollArea>
  )
}