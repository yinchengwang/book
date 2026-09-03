import { useState, KeyboardEvent } from 'react'
import { Button } from './ui/button'
import { Send, Loader2 } from 'lucide-react'

interface InputBoxProps {
  onSend: (message: string) => void
  isLoading: boolean
  placeholder?: string
}

export const InputBox = ({ onSend, isLoading, placeholder = '输入你的问题... (Enter 发送, Shift+Enter 换行)' }: InputBoxProps) => {
  const [input, setInput] = useState('')

  const handleSend = () => {
    if (input.trim() && !isLoading) {
      onSend(input.trim())
      setInput('')
    }
  }

  const handleKeyDown = (e: KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault()
      handleSend()
    }
  }

  return (
    <div className="border-t border-gray-200 bg-white p-4 dark:border-gray-700 dark:bg-gray-800">
      <div className="flex gap-2 max-w-4xl mx-auto">
        <textarea
          value={input}
          onChange={(e) => setInput(e.target.value)}
          onKeyDown={handleKeyDown}
          placeholder={placeholder}
          disabled={isLoading}
          className="flex-1 min-h-[44px] max-h-32 px-3 py-2 rounded-md border border-gray-300 bg-white text-sm resize-none focus:outline-none focus:ring-2 focus:ring-blue-500 dark:border-gray-600 dark:bg-gray-900"
          rows={1}
        />
        <Button
          onClick={handleSend}
          disabled={!input.trim() || isLoading}
          size="icon"
        >
          {isLoading ? (
            <Loader2 className="w-4 h-4 animate-spin" />
          ) : (
            <Send className="w-4 h-4" />
          )}
        </Button>
      </div>
    </div>
  )
}