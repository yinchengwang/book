'use client';

import { create } from 'zustand';
import type { RetrievalResult } from '@/lib/api';

export interface ChatMessage {
  id: string;
  role: 'user' | 'assistant' | 'system';
  content: string;
  context?: RetrievalResult[];
  retrievalTimeMs?: number;
  generationTimeMs?: number;
  totalTimeMs?: number;
  totalTokens?: number;
  timestamp: number;
  streaming?: boolean;
}

export interface Conversation {
  id: string;
  title: string;
  messages: ChatMessage[];
  createdAt: number;
  updatedAt: number;
  pipelineType?: string;
}

interface ChatState {
  conversations: Conversation[];
  currentConversationId: string | null;
  isStreaming: boolean;

  setCurrentConversation: (id: string | null) => void;
  createConversation: (title?: string) => string;
  deleteConversation: (id: string) => void;

  addMessage: (conversationId: string, message: ChatMessage) => void;
  updateMessage: (conversationId: string, messageId: string, updates: Partial<ChatMessage>) => void;
  clearMessages: (conversationId: string) => void;

  setStreaming: (streaming: boolean) => void;
}

export const useChatStore = create<ChatState>((set, get) => ({
  conversations: [],
  currentConversationId: null,
  isStreaming: false,

  setCurrentConversation: (id) => set({ currentConversationId: id }),

  createConversation: (title = '新对话') => {
    const id = `conv_${Date.now()}_${Math.random().toString(36).slice(2, 9)}`;
    const now = Date.now();
    set((state) => ({
      conversations: [
        {
          id,
          title,
          messages: [],
          createdAt: now,
          updatedAt: now,
        },
        ...state.conversations,
      ],
      currentConversationId: id,
    }));
    return id;
  },

  deleteConversation: (id) =>
    set((state) => ({
      conversations: state.conversations.filter((c) => c.id !== id),
      currentConversationId:
        state.currentConversationId === id ? null : state.currentConversationId,
    })),

  addMessage: (conversationId, message) =>
    set((state) => ({
      conversations: state.conversations.map((c) =>
        c.id === conversationId
          ? {
              ...c,
              messages: [...c.messages, message],
              updatedAt: Date.now(),
              // 用第一条用户消息作为标题
              title:
                c.title === '新对话' && message.role === 'user'
                  ? message.content.slice(0, 30) + (message.content.length > 30 ? '...' : '')
                  : c.title,
            }
          : c
      ),
    })),

  updateMessage: (conversationId, messageId, updates) =>
    set((state) => ({
      conversations: state.conversations.map((c) =>
        c.id === conversationId
          ? {
              ...c,
              messages: c.messages.map((m) =>
                m.id === messageId ? { ...m, ...updates } : m
              ),
              updatedAt: Date.now(),
            }
          : c
      ),
    })),

  clearMessages: (conversationId) =>
    set((state) => ({
      conversations: state.conversations.map((c) =>
        c.id === conversationId ? { ...c, messages: [], updatedAt: Date.now() } : c
      ),
    })),

  setStreaming: (streaming) => set({ isStreaming: streaming }),
}));