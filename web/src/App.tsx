import { useState, useCallback, useRef } from 'react'
import { QueryClient, QueryClientProvider } from '@tanstack/react-query'
import { Chat } from './components/Chat'
import { SettingsPanel, DEFAULT_SETTINGS } from './components/SettingsPanel'
import { ThemeToggle } from './components/ThemeToggle'
import { DocumentPreview } from './components/DocumentPreview'
import { Button } from './components/ui/button'
import { useContext } from './hooks/useContext'
import { useDocument } from './hooks/useDocument'
import { Settings, MessageSquare, RotateCcw, Loader2 } from 'lucide-react'
import type { Settings as SettingsType, QueryOptions, ChunkReference } from './types'

const queryClient = new QueryClient({
  defaultOptions: {
    queries: {
      retry: 1,
      refetchOnWindowFocus: false,
    },
  },
})

function App() {
  const [settings, setSettings] = useState<SettingsType>(() => {
    try {
      const saved = localStorage.getItem('rag-settings')
      return saved ? { ...DEFAULT_SETTINGS, ...JSON.parse(saved) } : DEFAULT_SETTINGS
    } catch {
      return DEFAULT_SETTINGS
    }
  })
  const [showSettings, setShowSettings] = useState(false)
  const [chatKey, setChatKey] = useState(0)

  const { buildQuery, recordTurn, clearContext } = useContext(settings.maxTurns)
  const { previewDocument, isLoading: isDocLoading, loadDocument, closePreview } = useDocument()

  // 暂存当前用户问题，收到回复后与答案一起记入上下文
  const pendingQuestionRef = useRef<string>('')

  const queryOptions: QueryOptions = {
    topK: settings.topK,
    temperature: settings.temperature,
    maxTokens: settings.maxTokens,
    useRerank: settings.useRerank
  }

  const handleBuildQuery = useCallback((content: string): string => {
    pendingQuestionRef.current = content
    return buildQuery(content)
  }, [buildQuery])

  const handleAssistantMessage = useCallback((answer: string) => {
    if (pendingQuestionRef.current) {
      recordTurn(pendingQuestionRef.current, answer)
      pendingQuestionRef.current = ''
    }
  }, [recordTurn])

  const handleChunkClick = useCallback((chunk: ChunkReference) => {
    // 优先用 document_id，退化为 file_path（后端两种都支持）
    loadDocument(chunk.document_id || chunk.file_path)
  }, [loadDocument])

  const handleNewChat = () => {
    clearContext()
    localStorage.removeItem('rag-chat-history')
    setChatKey(k => k + 1)  // 重新挂载 Chat，清空消息
  }

  return (
    <QueryClientProvider client={queryClient}>
      <div className="h-full flex flex-col bg-gray-50 dark:bg-gray-900">
        <header className="flex items-center justify-between px-4 py-3 bg-white border-b border-gray-200 dark:bg-gray-800 dark:border-gray-700">
          <div className="flex items-center gap-2">
            <MessageSquare className="w-6 h-6 text-blue-600" />
            <h1 className="text-lg md:text-xl font-bold">D-code-book RAG</h1>
          </div>

          <div className="flex items-center gap-1 md:gap-2">
            <Button variant="ghost" size="icon" onClick={handleNewChat} title="新对话">
              <RotateCcw className="w-5 h-5" />
            </Button>
            <ThemeToggle />
            <Button
              variant="ghost"
              size="icon"
              onClick={() => setShowSettings(!showSettings)}
              title="设置"
            >
              <Settings className="w-5 h-5" />
            </Button>
          </div>
        </header>

        <main className="flex-1 relative overflow-hidden">
          <Chat
            key={chatKey}
            options={queryOptions}
            buildQuery={handleBuildQuery}
            onAssistantMessage={handleAssistantMessage}
            onChunkClick={handleChunkClick}
          />

          {isDocLoading && (
            <div className="absolute inset-0 flex items-center justify-center bg-black/20 z-20">
              <Loader2 className="w-8 h-8 animate-spin text-white" />
            </div>
          )}

          {showSettings && (
            <SettingsPanel
              settings={settings}
              onChange={setSettings}
              onClose={() => setShowSettings(false)}
            />
          )}
        </main>

        <footer className="px-4 py-2 text-center text-xs text-gray-500 border-t border-gray-200 dark:text-gray-400 dark:border-gray-700">
          D-code-book RAG
        </footer>

        {previewDocument && (
          <DocumentPreview
            doc={previewDocument}
            onClose={closePreview}
          />
        )}
      </div>
    </QueryClientProvider>
  )
}

export default App
