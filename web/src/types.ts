// 消息
export interface Message {
  id: string;
  role: 'user' | 'assistant';
  content: string;
  timestamp: number;
  chunks?: ChunkReference[];
  isStreaming?: boolean;
}

// 文档引用（document_id 用于加载完整文档预览）
export interface ChunkReference {
  id: string;
  document_id?: string;
  content: string;
  file_path: string;
  score: number;
}

// 查询响应（匹配后端 /api/v1/query 实际输出）
export interface QueryResponse {
  answer: string;
  chunks: ChunkReference[];
  confidence: number;
  query_time_ms: number;
  request_id?: string;
}

// 检索响应（匹配后端 /api/v1/retrieve 实际输出）
export interface RetrieveResponse {
  results: ChunkReference[];
}

// 查询选项（当前后端仅 topK 生效，其余持久化备用）
export interface QueryOptions {
  topK: number;
  temperature: number;
  maxTokens: number;
  useRerank: boolean;
}

// 设置（持久化到 localStorage）
export interface Settings {
  topK: number;
  minScore: number;
  temperature: number;
  maxTokens: number;
  useRerank: boolean;
  maxTurns: number;
}

// 文档内容（匹配后端 /api/v1/documents/{id}/content 输出）
export interface DocumentContent {
  id: string;
  file_path: string;
  content: string;
  title: string;
}
