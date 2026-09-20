import type { Metadata } from 'next';
import './globals.css';

export const metadata: Metadata = {
  title: 'Modular RAG - 智能问答系统',
  description: '生产级模块化 RAG 框架，支持 9 种 RAG 类型',
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="zh-CN" suppressHydrationWarning>
      <body className="min-h-screen bg-background font-sans antialiased">
        {children}
      </body>
    </html>
  );
}