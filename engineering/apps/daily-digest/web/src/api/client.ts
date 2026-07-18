const BASE_URL = '/api/v1'

interface FetchOptions extends RequestInit {
  params?: Record<string, string | number | undefined>
}

async function request<T>(path: string, options: FetchOptions = {}): Promise<T> {
  const { params, ...init } = options

  let url = `${BASE_URL}${path}`
  if (params) {
    const searchParams = new URLSearchParams()
    Object.entries(params).forEach(([key, value]) => {
      if (value !== undefined && value !== '') {
        searchParams.append(key, String(value))
      }
    })
    const qs = searchParams.toString()
    if (qs) url += `?${qs}`
  }

  const response = await fetch(url, {
    headers: {
      'Content-Type': 'application/json',
      ...init.headers,
    },
    ...init,
  })

  if (!response.ok) {
    const error = await response.json().catch(() => ({ detail: '请求失败' }))
    throw new Error(error.detail || `HTTP ${response.status}`)
  }

  return response.json()
}

export interface Item {
  id: number
  source: string
  source_id: string
  title: string
  url: string
  summary: string
  raw_content: string
  category: string
  tags: string[]
  score: number
  published: string | null
  source_weight: number
  created_at: string
}

export interface ItemListResponse {
  items: Item[]
  total: number
  page: number
  size: number
}

export interface Collection {
  id: number
  user_id: number
  name: string
  description: string
  item_count: number
  created_at: string
}

export interface Subscription {
  id: number
  user_id: number
  category: string
  keywords: string
  weight: number
  created_at: string
}

export const api = {
  getDaily(category?: string, page = 1, size = 20) {
    return request<ItemListResponse>('/daily', {
      params: { category, page, size },
    })
  },

  getItem(id: number) {
    return request<Item>(`/daily/${id}`)
  },

  search(q: string, category?: string, page = 1, size = 20) {
    return request<ItemListResponse>('/search', {
      params: { q, category, page, size },
    })
  },

  getCollections(userId: number) {
    return request<Collection[]>('/collections', { params: { user_id: userId } })
  },

  createCollection(userId: number, name: string, description = '') {
    return request<Collection>('/collections', {
      method: 'POST',
      body: JSON.stringify({ user_id: userId, name, description }),
    })
  },

  deleteCollection(id: number) {
    return request<void>(`/collections/${id}`, { method: 'DELETE' })
  },

  getStats() {
    return request<any>('/stats')
  },

  translate(text: string) {
    return request<{ translated_text: string }>('/translate', {
      method: 'POST',
      body: JSON.stringify({ text }),
    })
  },
}
