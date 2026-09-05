// shared/web/src/components/Markdown.tsx
// Cross-project Markdown renderer (react-markdown + remark-gfm).
//
// Deps required by consumers:
//   - react-markdown@^9.0.0
//   - remark-gfm@^4.0.0
// reading-radar already has both. Games-web should install them
// before importing this component (see games-web/package.json).
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';

export function Markdown({ content }: { content: string }) {
  return (
    <div className="prose dark:prose-invert max-w-none">
      <ReactMarkdown remarkPlugins={[remarkGfm]}>{content}</ReactMarkdown>
    </div>
  );
}
