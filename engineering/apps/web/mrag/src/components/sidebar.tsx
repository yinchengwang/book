'use client';

import { useChatStore } from '@/stores/chat-store';
import { useUIStore } from '@/stores/ui-store';
import { Button } from '@/components/ui/button';
import { Plus, MessageSquare, Trash2, Database, BarChart3, Settings, Activity } from 'lucide-react';
import { cn } from '@/lib/utils';

interface SidebarProps {
  activeView: string;
  onViewChange: (view: string) => void;
}

export function Sidebar({ activeView, onViewChange }: SidebarProps) {
  const {
    conversations,
    currentConversationId,
    createConversation,
    deleteConversation,
    setCurrentConversation,
  } = useChatStore();

  const navItems = [
    { id: 'chat', label: '对话', icon: MessageSquare },
    { id: 'knowledge', label: '知识库', icon: Database },
    { id: 'pipeline', label: 'Pipeline', icon: Settings },
    { id: 'evaluation', label: '评估', icon: BarChart3 },
    { id: 'monitor', label: '监控', icon: Activity },
  ];

  return (
    <aside className="w-64 border-r bg-card flex flex-col h-full">
      {/* Logo */}
      <div className="p-4 border-b">
        <h1 className="text-lg font-bold gradient-text">Modular RAG</h1>
        <p className="text-xs text-muted-foreground">智能问答系统</p>
      </div>

      {/* 导航 */}
      <nav className="p-2 border-b">
        {navItems.map((item) => {
          const Icon = item.icon;
          return (
            <button
              key={item.id}
              onClick={() => onViewChange(item.id)}
              className={cn(
                'w-full flex items-center gap-2 px-3 py-2 rounded-md text-sm transition-colors',
                activeView === item.id
                  ? 'bg-primary text-primary-foreground'
                  : 'hover:bg-accent text-muted-foreground hover:text-foreground'
              )}
            >
              <Icon className="h-4 w-4" />
              {item.label}
            </button>
          );
        })}
      </nav>

      {/* 对话列表（仅在 chat 视图显示） */}
      {activeView === 'chat' && (
        <div className="flex-1 overflow-hidden flex flex-col">
          <div className="p-2 border-b">
            <Button
              onClick={() => createConversation('新对话')}
              className="w-full"
              size="sm"
              variant="outline"
            >
              <Plus className="h-4 w-4 mr-2" />
              新建对话
            </Button>
          </div>

          <div className="flex-1 overflow-y-auto p-2 space-y-1 scrollbar-thin">
            {conversations.map((conv) => (
              <div
                key={conv.id}
                onClick={() => setCurrentConversation(conv.id)}
                className={cn(
                  'group flex items-center justify-between px-3 py-2 rounded-md text-sm cursor-pointer transition-colors',
                  currentConversationId === conv.id
                    ? 'bg-accent text-foreground'
                    : 'hover:bg-accent/50 text-muted-foreground'
                )}
              >
                <span className="truncate flex-1">{conv.title}</span>
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    deleteConversation(conv.id);
                  }}
                  className="opacity-0 group-hover:opacity-100 transition-opacity"
                >
                  <Trash2 className="h-3 w-3 hover:text-destructive" />
                </button>
              </div>
            ))}
            {conversations.length === 0 && (
              <div className="text-center text-xs text-muted-foreground mt-4">
                还没有对话
              </div>
            )}
          </div>
        </div>
      )}
    </aside>
  );
}