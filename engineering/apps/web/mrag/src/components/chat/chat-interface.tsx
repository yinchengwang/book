'use client';

import { useState, useRef, useEffect } from 'react';
import { Button } from '@/components/ui/button';
import { Textarea } from '@/components/ui/textarea';
import { Card } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { useChatStore, type ChatMessage } from '@/stores/chat-store';
import { useUIStore } from '@/stores/ui-store';
import { apiClient } from '@/lib/api';
import { formatLatency, generateId } from '@/lib/utils';
import { Send, Loader2, User, Bot, ChevronRight } from 'lucide-react';

export function ChatInterface() {
  const [input, setInput] = useState('');
  const messagesEndRef = useRef<HTMLDivElement>(null);

  const {
    conversations,
    currentConversationId,
    isStreaming,
    createConversation,
    addMessage,
    updateMessage,
    setStreaming,
  } = useChatStore();

  const { selectedPipeline } = useUIStore();

  const currentConversation = conversations.find((c) => c.id === currentConversationId);

  // 自动滚动到底部
  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [currentConversation?.messages]);

  // 首次加载时创建对话
  useEffect(() => {
    if (!currentConversationId) {
      createConversation();
    }
  }, [currentConversationId, createConversation]);

  const handleSend = async () => {
    if (!input.trim() || isStreaming) return;

    let convId = currentConversationId;
    if (!convId) {
      convId = createConversation();
    }

    const userMessage: ChatMessage = {
      id: generateId(),
      role: 'user',
      content: input.trim(),
      timestamp: Date.now(),
    };

    addMessage(convId, userMessage);
    setInput('');
    setStreaming(true);

    // 添加一个空的 assistant 消息用于流式填充
    const assistantId = generateId();
    const assistantMessage: ChatMessage = {
      id: assistantId,
      role: 'assistant',
      content: '',
      timestamp: Date.now(),
      streaming: true,
    };
    addMessage(convId, assistantMessage);

    try {
      const response = await apiClient.query({
        query: userMessage.content,
        pipeline_type: selectedPipeline,
        top_k: 5,
      });

      updateMessage(convId, assistantId, {
        content: response.answer,
        context: response.context,
        retrievalTimeMs: response.retrieval_time_ms,
        generationTimeMs: response.generation_time_ms,
        totalTimeMs: response.total_time_ms,
        totalTokens: response.total_tokens,
        streaming: false,
      });
    } catch (error) {
      updateMessage(convId, assistantId, {
        content: `错误: ${error instanceof Error ? error.message : '未知错误'}`,
        streaming: false,
      });
    } finally {
      setStreaming(false);
    }
  };

  const handleKeyDown = (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleSend();
    }
  };

  return (
    <div className="flex flex-col h-full">
      {/* 消息列表 */}
      <div className="flex-1 overflow-y-auto p-4 space-y-4 scrollbar-thin">
        {currentConversation?.messages.length === 0 && (
          <div className="flex items-center justify-center h-full text-muted-foreground">
            <div className="text-center">
              <Bot className="mx-auto h-12 w-12 mb-4 opacity-50" />
              <h2 className="text-xl font-semibold mb-2">开始提问</h2>
              <p className="text-sm">支持 9 种 RAG Pipeline 类型，使用当前选择的 Pipeline 回答</p>
            </div>
          </div>
        )}

        {currentConversation?.messages.map((message) => (
          <ChatMessageBubble key={message.id} message={message} />
        ))}

        <div ref={messagesEndRef} />
      </div>

      {/* 输入框 */}
      <div className="border-t p-4 bg-background">
        <div className="flex gap-2 items-end">
          <Textarea
            value={input}
            onChange={(e) => setInput(e.target.value)}
            onKeyDown={handleKeyDown}
            placeholder="输入你的问题... (Shift+Enter 换行)"
            disabled={isStreaming}
            className="min-h-[60px] max-h-[200px] resize-none"
          />
          <Button
            onClick={handleSend}
            disabled={!input.trim() || isStreaming}
            size="icon"
            className="h-[60px] w-[60px]"
          >
            {isStreaming ? (
              <Loader2 className="h-5 w-5 animate-spin" />
            ) : (
              <Send className="h-5 w-5" />
            )}
          </Button>
        </div>
        <div className="mt-2 text-xs text-muted-foreground">
          当前 Pipeline: <Badge variant="secondary">{selectedPipeline}</Badge>
        </div>
      </div>
    </div>
  );
}

function ChatMessageBubble({ message }: { message: ChatMessage }) {
  const isUser = message.role === 'user';

  return (
    <div className={`flex gap-3 ${isUser ? 'justify-end' : 'justify-start'}`}>
      {!isUser && (
        <div className="flex-shrink-0 w-8 h-8 rounded-full bg-primary/10 flex items-center justify-center">
          <Bot className="h-4 w-4 text-primary" />
        </div>
      )}

      <div className={`flex-1 max-w-[80%] ${isUser ? 'flex justify-end' : ''}`}>
        <Card className={`p-4 ${isUser ? 'bg-primary text-primary-foreground' : 'bg-muted'}`}>
          <div className="whitespace-pre-wrap break-words">
            {message.content || (message.streaming ? '...' : '')}
          </div>

          {/* 检索上下文 */}
          {!isUser && message.context && message.context.length > 0 && (
            <details className="mt-3 text-xs">
              <summary className="cursor-pointer text-muted-foreground hover:text-foreground flex items-center gap-1">
                <ChevronRight className="h-3 w-3" />
                检索到 {message.context.length} 个相关文档
              </summary>
              <div className="mt-2 space-y-2">
                {message.context.map((doc, idx) => (
                  <div
                    key={doc.chunk_id}
                    className="p-2 rounded bg-background/50 border"
                  >
                    <div className="flex items-center justify-between mb-1">
                      <Badge variant="outline" className="text-xs">
                        来源 {idx + 1}
                      </Badge>
                      <Badge
                        variant={doc.score > 0.8 ? 'success' : doc.score > 0.5 ? 'warning' : 'secondary'}
                        className="text-xs"
                      >
                        相似度 {(doc.score * 100).toFixed(1)}%
                      </Badge>
                    </div>
                    <div className="text-xs text-muted-foreground line-clamp-3">
                      {doc.content}
                    </div>
                  </div>
                ))}
              </div>
            </details>
          )}

          {/* 性能指标 */}
          {!isUser && !message.streaming && message.totalTimeMs !== undefined && (
            <div className="mt-3 flex gap-3 text-xs text-muted-foreground">
              <span>检索: {formatLatency(message.retrievalTimeMs || 0)}</span>
              <span>生成: {formatLatency(message.generationTimeMs || 0)}</span>
              <span>总耗时: {formatLatency(message.totalTimeMs || 0)}</span>
              {message.totalTokens !== undefined && (
                <span>Tokens: {message.totalTokens}</span>
              )}
            </div>
          )}
        </Card>
      </div>

      {isUser && (
        <div className="flex-shrink-0 w-8 h-8 rounded-full bg-primary flex items-center justify-center">
          <User className="h-4 w-4 text-primary-foreground" />
        </div>
      )}
    </div>
  );
}