import { useState, useEffect } from 'react'
import { Card, CardContent, CardHeader, CardTitle } from './ui/card'
import { Slider } from './ui/slider'
import { Switch } from './ui/switch'
import { Button } from './ui/button'
import { X, RotateCcw } from 'lucide-react'
import type { Settings } from '@/types'

export const DEFAULT_SETTINGS: Settings = {
  topK: 5,
  minScore: 0.1,
  temperature: 0.7,
  maxTokens: 1024,
  useRerank: true,
  maxTurns: 5
}

interface SettingsPanelProps {
  settings: Settings
  onChange: (settings: Settings) => void
  onClose: () => void
}

export const SettingsPanel = ({ settings, onChange, onClose }: SettingsPanelProps) => {
  const [localSettings, setLocalSettings] = useState<Settings>(settings)

  useEffect(() => {
    setLocalSettings(settings)
  }, [settings])

  const handleSave = () => {
    onChange(localSettings)
    localStorage.setItem('rag-settings', JSON.stringify(localSettings))
    onClose()
  }

  const handleReset = () => {
    setLocalSettings(DEFAULT_SETTINGS)
  }

  return (
    <div className="absolute right-0 top-0 h-full w-full sm:w-80 bg-white border-l border-gray-200 shadow-lg z-10 dark:bg-gray-800 dark:border-gray-700">
      <div className="flex items-center justify-between p-4 border-b border-gray-200 dark:border-gray-700">
        <h2 className="text-lg font-semibold">设置</h2>
        <button
          onClick={onClose}
          className="text-gray-500 hover:text-gray-700 dark:hover:text-gray-300"
        >
          <X className="w-5 h-5" />
        </button>
      </div>

      <div className="p-4 space-y-4 overflow-y-auto h-[calc(100%-130px)]">
        <Card>
          <CardHeader className="pb-2">
            <CardTitle className="text-sm">检索配置</CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div>
              <label className="text-sm text-gray-600 dark:text-gray-400">
                Top K: {localSettings.topK}
              </label>
              <Slider
                value={[localSettings.topK]}
                onValueChange={([v]) => setLocalSettings(s => ({ ...s, topK: v }))}
                min={1}
                max={50}
                step={1}
                className="mt-2"
              />
            </div>

            <div>
              <label className="text-sm text-gray-600 dark:text-gray-400">
                最小分数: {localSettings.minScore.toFixed(2)}
              </label>
              <Slider
                value={[Math.round(localSettings.minScore * 100)]}
                onValueChange={([v]) => setLocalSettings(s => ({ ...s, minScore: v / 100 }))}
                min={0}
                max={100}
                step={5}
                className="mt-2"
              />
            </div>

            <div className="flex items-center justify-between">
              <label className="text-sm text-gray-600 dark:text-gray-400">
                启用 Rerank
              </label>
              <Switch
                checked={localSettings.useRerank}
                onCheckedChange={(checked) => setLocalSettings(s => ({ ...s, useRerank: checked }))}
              />
            </div>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="pb-2">
            <CardTitle className="text-sm">生成配置</CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div>
              <label className="text-sm text-gray-600 dark:text-gray-400">
                温度: {localSettings.temperature.toFixed(2)}
              </label>
              <Slider
                value={[Math.round(localSettings.temperature * 100)]}
                onValueChange={([v]) => setLocalSettings(s => ({ ...s, temperature: v / 100 }))}
                min={0}
                max={200}
                step={10}
                className="mt-2"
              />
            </div>

            <div>
              <label className="text-sm text-gray-600 dark:text-gray-400">
                最大 Token: {localSettings.maxTokens}
              </label>
              <Slider
                value={[localSettings.maxTokens]}
                onValueChange={([v]) => setLocalSettings(s => ({ ...s, maxTokens: v }))}
                min={128}
                max={4096}
                step={128}
                className="mt-2"
              />
            </div>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="pb-2">
            <CardTitle className="text-sm">对话配置</CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div>
              <label className="text-sm text-gray-600 dark:text-gray-400">
                上下文轮数: {localSettings.maxTurns}
              </label>
              <Slider
                value={[localSettings.maxTurns]}
                onValueChange={([v]) => setLocalSettings(s => ({ ...s, maxTurns: v }))}
                min={0}
                max={20}
                step={1}
                className="mt-2"
              />
              <p className="text-xs text-gray-400 mt-1">0 = 不携带历史对话</p>
            </div>
          </CardContent>
        </Card>
      </div>

      <div className="absolute bottom-0 left-0 right-0 p-4 border-t border-gray-200 bg-white flex gap-2 dark:border-gray-700 dark:bg-gray-800">
        <Button variant="outline" onClick={handleReset} className="flex-1">
          <RotateCcw className="w-4 h-4 mr-1" />
          重置
        </Button>
        <Button onClick={handleSave} className="flex-1">
          保存
        </Button>
      </div>
    </div>
  )
}
