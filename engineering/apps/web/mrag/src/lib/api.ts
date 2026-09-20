/**
 * API 客户端
 *
 * 与后端 C++ RAG 服务通信的统一接口
 */

// 注意：NEXT_PUBLIC_API_URL 为空时使用 '/api/v1'，通过 Next.js proxy 转发
const API_BASE_URL = process.env.NEXT_PUBLIC_API_URL || '/api/v1';

// ========== 类型定义 ==========

export interface QueryRequest {
  query: string;
  pipeline_type?: string;
  top_k?: number;
  options?: Record<string, unknown>;
}

export interface RetrievalResult {
  chunk_id: string;
  content: string;
  score: number;
  source: string;
  metadata?: Record<string, unknown>;
}

export interface QueryResponse {
  answer: string;
  context: RetrievalResult[];
  retrieval_time_ms: number;
  generation_time_ms: number;
  total_time_ms: number;
  total_tokens: number;
}

export interface Dataset {
  dataset_id: string;
  name: string;
  description: string;
  version: string;
  question_count: number;
  created_at: number;
  updated_at: number;
  latest_version: string;
  checksum: string;
}

export interface DatasetVersion {
  dataset_id: string;
  version: string;
  created_at: number;
  description: string;
  question_count: number;
  checksum: string;
}

export interface Question {
  id: string;
  query: string;
  ground_truth: string;
  relevant_doc_ids: string[];
  key_facts: string[];
  category: string;
  difficulty: string;
}

export interface EvalRun {
  run_id: string;
  dataset_id: string;
  dataset_version: string;
  pipeline_type: string;
  timestamp: number;
  author: string;
  description: string;
  tags: string[];
  metrics: Record<string, number>;
  stats: {
    total_questions: number;
    successful_questions: number;
    failed_questions: number;
  };
}

export interface ComparisonResult {
  comparison_id: string;
  run_ids: string[];
  pipeline_types: string[];
  metric_diffs: Array<{
    metric_name: string;
    value_a: number;
    value_b: number;
    delta: number;
    percent_change: number;
    direction: 'up' | 'down' | 'same';
  }>;
  timestamp: number;
}

export interface OptimizationIssue {
  category: string;
  severity: 'low' | 'medium' | 'high' | 'critical';
  title: string;
  description: string;
  metric_name: string;
  metric_value: number;
  threshold: number;
}

export interface OptimizationAction {
  action_name: string;
  related_issue: string;
  description: string;
  parameter_name: string;
  new_value: string;
  expected_impact: string;
  priority: 'low' | 'medium' | 'high' | 'critical';
  steps: string[];
}

export interface OptimizationPlan {
  plan_id: string;
  source_run_id: string;
  timestamp: number;
  overall_score: number;
  overall_assessment: string;
  issues: OptimizationIssue[];
  priority_actions: OptimizationAction[];
  category_scores: Record<string, number>;
}

export interface UploadTask {
  task_id: string;
  filename: string;
  size: number;
  stage: 'queued' | 'parsing' | 'chunking' | 'embedding' | 'indexing' | 'completed' | 'failed';
  progress: number;
  processed: number;
  total: number;
  elapsed_ms: number;
  chunk_count: number;
  doc_id?: string;
  error?: string;
  completed: boolean;
}

export interface Document {
  id: string;
  name: string;
  type: string;
  size: number;
  status: 'pending' | 'processing' | 'indexed' | 'failed';
  uploaded_at: number;
  chunk_count?: number;
  error_message?: string;
}

export interface PipelineConfig {
  type: 'naive' | 'advanced' | 'hybrid' | 'hyde' | 'graph' | 'corrective' | 'react' | 'iterative' | 'recursive';
  retrieval: {
    top_k: number;
    rrf_k?: number;
    enable_reranker?: boolean;
  };
  llm: {
    model: string;
    temperature: number;
    max_tokens: number;
  };
}

export interface SystemStatus {
  status: 'healthy' | 'degraded' | 'unavailable';
  uptime_seconds: number;
  total_queries: number;
  total_documents: number;
  total_chunks: number;
  active_pipelines: string[];
  models_loaded: string[];
}

// ========== API 客户端类 ==========

class ApiClient {
  private baseUrl: string;

  constructor(baseUrl: string = API_BASE_URL) {
    this.baseUrl = baseUrl;
  }

  private async request<T>(
    endpoint: string,
    options: RequestInit = {}
  ): Promise<T> {
    const url = `${this.baseUrl}${endpoint}`;
    const response = await fetch(url, {
      headers: {
        'Content-Type': 'application/json',
        ...options.headers,
      },
      ...options,
    });

    if (!response.ok) {
      throw new Error(`API error: ${response.status} ${response.statusText}`);
    }

    return response.json();
  }

  // ========== Query API ==========

  async query(request: QueryRequest): Promise<QueryResponse> {
    return this.request<QueryResponse>('/query', {
      method: 'POST',
      body: JSON.stringify(request),
    });
  }

  async *queryStream(request: QueryRequest): AsyncGenerator<string> {
    const url = `${this.baseUrl}/query/stream`;
    const response = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(request),
    });

    if (!response.ok) {
      throw new Error(`Stream error: ${response.status}`);
    }

    const reader = response.body?.getReader();
    if (!reader) throw new Error('No response body');

    const decoder = new TextDecoder();
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      yield decoder.decode(value);
    }
  }

  // ========== Knowledge Base API ==========

  async listDocuments(): Promise<Document[]> {
    // 通过 Next.js proxy 转发，避免 CORS 问题
    const response = await this.request<{ documents: any[]; total: number }>('/knowledge/documents');

    // 字段映射: file_name → name, file_size → size, indexed_at → uploaded_at
    const statusMap: Record<number, Document['status']> = {
      0: 'pending',
      1: 'processing',
      2: 'indexed',
      3: 'failed',
    };

    return (response.documents || []).map((doc: any) => ({
      id: doc.id,
      name: doc.file_name,
      type: doc.file_type,
      size: doc.file_size,
      status: statusMap[doc.status] || 'pending',
      uploaded_at: doc.indexed_at || 0,
      chunk_count: doc.chunk_count,
      error_message: doc.error_message,
    }));
  }

  async uploadDocument(
    file: File,
    onProgress?: (loaded: number, total: number) => void
  ): Promise<UploadTask> {
    // 使用 XMLHttpRequest 而非 fetch —— 因为 fetch 没有上传进度回调
    // 直接访问后端 (Next.js rewrites 对 multipart/form-data 的转发有问题)
    const backendBase = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:8080';

    return new Promise<UploadTask>((resolve, reject) => {
      const xhr = new XMLHttpRequest();

      // 上传进度回调
      xhr.upload.addEventListener('progress', (e) => {
        if (e.lengthComputable && onProgress) {
          onProgress(e.loaded, e.total);
        }
      });

      xhr.addEventListener('load', () => {
        if (xhr.status >= 200 && xhr.status < 300) {
          try {
            resolve(JSON.parse(xhr.responseText));
          } catch {
            reject(new Error(`Failed to parse response: ${xhr.responseText}`));
          }
        } else {
          reject(new Error(`Upload failed: ${xhr.status} ${xhr.statusText}`));
        }
      });

      xhr.addEventListener('error', () => {
        reject(new Error(`Upload network error`));
      });

      xhr.addEventListener('abort', () => {
        reject(new Error(`Upload aborted`));
      });

      const formData = new FormData();
      formData.append('file', file);

      xhr.open('POST', `${backendBase}/api/v1/knowledge/upload`, true);
      xhr.send(formData);
    });
  }

  // 轮询上传任务进度
  async getUploadProgress(taskId: string): Promise<UploadTask> {
    const backendBase = process.env.NEXT_PUBLIC_API_URL || 'http://localhost:8080';
    const response = await fetch(`${backendBase}/api/v1/knowledge/upload/${taskId}`, {
      mode: 'cors',
    });
    if (!response.ok) {
      throw new Error(`Failed to get progress: ${response.statusText}`);
    }
    return response.json();
  }

  async deleteDocument(id: string): Promise<void> {
    // 通过 Next.js proxy 转发，避免 CORS 问题
    await this.request(`/knowledge/documents/${id}`, {
      method: 'DELETE',
    });
  }

  // ========== Pipeline API ==========

  async listPipelines(): Promise<string[]> {
    return this.request<string[]>('/pipelines');
  }

  async getPipelineConfig(type: string): Promise<PipelineConfig> {
    return this.request<PipelineConfig>(`/pipelines/${type}`);
  }

  async updatePipelineConfig(type: string, config: PipelineConfig): Promise<void> {
    await this.request(`/pipelines/${type}`, {
      method: 'PUT',
      body: JSON.stringify(config),
    });
  }

  // ========== Evaluation API ==========

  // Datasets
  async listDatasets(): Promise<Dataset[]> {
    return this.request<Dataset[]>('/eval/datasets');
  }

  async createDataset(name: string, description: string, questions: Question[]): Promise<Dataset> {
    return this.request<Dataset>('/eval/datasets', {
      method: 'POST',
      body: JSON.stringify({ name, description, questions }),
    });
  }

  async getDataset(id: string, version?: string): Promise<Dataset> {
    const qs = version ? `?version=${version}` : '';
    return this.request<Dataset>(`/eval/datasets/${id}${qs}`);
  }

  async deleteDataset(id: string): Promise<void> {
    await this.request(`/eval/datasets/${id}`, {
      method: 'DELETE',
    });
  }

  // Runs
  async listRuns(): Promise<EvalRun[]> {
    return this.request<EvalRun[]>('/eval/runs');
  }

  async createRun(
    datasetId: string,
    datasetVersion: string,
    pipelineType: string,
    pipelineConfig: PipelineConfig,
    description?: string
  ): Promise<EvalRun> {
    return this.request<EvalRun>('/eval/runs', {
      method: 'POST',
      body: JSON.stringify({
        dataset_id: datasetId,
        dataset_version: datasetVersion,
        pipeline_type: pipelineType,
        pipeline_config: pipelineConfig,
        description,
      }),
    });
  }

  async getRun(runId: string): Promise<EvalRun> {
    return this.request<EvalRun>(`/eval/runs/${runId}`);
  }

  async compareRuns(runIdA: string, runIdB: string): Promise<ComparisonResult> {
    return this.request<ComparisonResult>('/eval/compare', {
      method: 'POST',
      body: JSON.stringify({ run_id_a: runIdA, run_id_b: runIdB }),
    });
  }

  async getMetricTrend(
    pipelineType: string,
    metricName: string,
    limit = 30
  ): Promise<Array<{ timestamp: number; value: number }>> {
    return this.request<Array<{ timestamp: number; value: number }>>(
      `/eval/trends?pipeline_type=${pipelineType}&metric_name=${metricName}&limit=${limit}`
    );
  }

  async getOptimizationSuggestions(runId: string): Promise<OptimizationPlan> {
    return this.request<OptimizationPlan>(`/eval/optimization-suggestions/${runId}`);
  }

  // ========== System API ==========

  async getStatus(): Promise<SystemStatus> {
    return this.request<SystemStatus>('/status');
  }

  async getMetrics(): Promise<Record<string, unknown>> {
    return this.request<Record<string, unknown>>('/metrics');
  }
}

export const apiClient = new ApiClient();
export default apiClient;