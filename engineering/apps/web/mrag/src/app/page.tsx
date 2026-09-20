'use client';

import { useState } from 'react';
import { Sidebar } from '@/components/sidebar';
import { ChatInterface } from '@/components/chat/chat-interface';
import { KnowledgeView } from '@/components/knowledge/knowledge-view';
import { PipelineView } from '@/components/pipeline/pipeline-view';
import { EvaluationView } from '@/components/evaluation/evaluation-view';
import { MonitorView } from '@/components/monitor/monitor-view';

export default function HomePage() {
  const [activeView, setActiveView] = useState('chat');

  const renderView = () => {
    switch (activeView) {
      case 'knowledge':
        return <KnowledgeView />;
      case 'pipeline':
        return <PipelineView />;
      case 'evaluation':
        return <EvaluationView />;
      case 'monitor':
        return <MonitorView />;
      case 'chat':
      default:
        return <ChatInterface />;
    }
  };

  return (
    <div className="flex h-screen bg-background">
      <Sidebar activeView={activeView} onViewChange={setActiveView} />
      <main className="flex-1 flex flex-col overflow-hidden">
        {renderView()}
      </main>
    </div>
  );
}