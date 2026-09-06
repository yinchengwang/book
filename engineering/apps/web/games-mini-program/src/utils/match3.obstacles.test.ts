/**
 * @file match3.obstacles.test.ts
 * @brief 消消乐障碍物系统单元测试（数据模型 + HP 系统 + 渲染）
 */
import { describe, it, expect } from 'vitest'
import {
  BOARD_SIZE,
  OBSTACLE_INFO,
  OBSTACLE_RENDER_INFO,
  createBoard,
  createEmptyCell,
  createGemCell,
  createGameState,
  damageObstacle,
  isObstacleDestroyed,
  clearObstacle,
  getObstacleRenderInfo,
  placeObstacles,
  getLevelConfig,
} from './match3'
import type { Cell, ObstacleRenderInfo, ObstacleType } from './match3'

describe('障碍物系统 - 数据模型', () => {
  describe('OBSTACLE_INFO 配置', () => {
    it('应包含 ice 类型', () => {
      expect(OBSTACLE_INFO.ice).toBeDefined()
      expect(OBSTACLE_INFO.ice.emoji).toBeTruthy()
      expect(OBSTACLE_INFO.ice.hits).toBeGreaterThan(0)
    })

    it('应包含 stone 类型', () => {
      expect(OBSTACLE_INFO.stone).toBeDefined()
      expect(OBSTACLE_INFO.stone.emoji).toBeTruthy()
      expect(OBSTACLE_INFO.stone.hits).toBeGreaterThan(0)
    })

    it('应包含 web 类型', () => {
      expect(OBSTACLE_INFO.web).toBeDefined()
      expect(OBSTACLE_INFO.web.emoji).toBeTruthy()
      expect(OBSTACLE_INFO.web.hits).toBeGreaterThan(0)
    })

    it('三种障碍物至少有两种不同的 hp', () => {
      const hps = [
        OBSTACLE_INFO.ice.hits,
        OBSTACLE_INFO.stone.hits,
        OBSTACLE_INFO.web.hits,
      ]
      const unique = new Set(hps)
      expect(unique.size).toBeGreaterThanOrEqual(2)
    })

    it('OBSTACLE_INFO 的 hits 必须为正整数', () => {
      const types: NonNullable<ObstacleType>[] = ['ice', 'stone', 'web']
      for (const t of types) {
        expect(Number.isInteger(OBSTACLE_INFO[t].hits)).toBe(true)
        expect(OBSTACLE_INFO[t].hits).toBeGreaterThan(0)
      }
    })

    it('OBSTACLE_RENDER_INFO 应为每种障碍物提供颜色', () => {
      const types: NonNullable<ObstacleType>[] = ['ice', 'stone', 'web']
      for (const t of types) {
        expect(OBSTACLE_RENDER_INFO[t]).toBeDefined()
        expect(OBSTACLE_RENDER_INFO[t].color).toMatch(/^#[0-9A-Fa-f]{6}$|^rgba\(/)
      }
    })
  })

  describe('关卡配置中的障碍物', () => {
    it('level 1 应配置冰块障碍物', () => {
      const cfg = getLevelConfig(1)
      const ice = cfg.obstacles.find(o => o.type === 'ice')
      expect(ice).toBeDefined()
      expect(ice!.count).toBeGreaterThan(0)
    })

    it('createGameState 初始化的 level 1 棋盘上应有 ice 障碍', () => {
      const state = createGameState(1)
      let iceCount = 0
      for (let r = 0; r < BOARD_SIZE; r++) {
        for (let c = 0; c < BOARD_SIZE; c++) {
          if (state.board[r][c].obstacle === 'ice') iceCount++
        }
      }
      expect(iceCount).toBeGreaterThan(0)
    })

    it('createGameState 初始化的 ice 障碍应填满初始 hp', () => {
      const state = createGameState(1)
      for (let r = 0; r < BOARD_SIZE; r++) {
        for (let c = 0; c < BOARD_SIZE; c++) {
          const cell = state.board[r][c]
          if (cell.obstacle === 'ice') {
            expect(cell.obstacleHp).toBe(OBSTACLE_INFO.ice.hits)
          }
        }
      }
    })
  })
})

describe('障碍物系统 - HP 系统', () => {
  describe('damageObstacle', () => {
    it('应按 amount 减少障碍物 hp', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'stone'
      cell.obstacleHp = OBSTACLE_INFO.stone.hits
      damageObstacle(cell, 2)
      expect(cell.obstacleHp).toBe(OBSTACLE_INFO.stone.hits - 2)
    })

    it('默认 amount 应为 1', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'web'
      cell.obstacleHp = OBSTACLE_INFO.web.hits
      damageObstacle(cell)
      expect(cell.obstacleHp).toBe(OBSTACLE_INFO.web.hits - 1)
    })

    it('hp 不应低于 0', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'ice'
      cell.obstacleHp = 1
      damageObstacle(cell, 999)
      expect(cell.obstacleHp).toBe(0)
    })

    it('无障碍物时不应修改 cell', () => {
      const cell = createGemCell(3)
      const snapshot = { ...cell }
      damageObstacle(cell, 1)
      expect(cell.gem).toBe(snapshot.gem)
      expect(cell.obstacle).toBe(snapshot.obstacle)
      expect(cell.obstacleHp).toBe(snapshot.obstacleHp)
    })

    it('amount 为 0 时不应修改 hp', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'stone'
      cell.obstacleHp = OBSTACLE_INFO.stone.hits
      damageObstacle(cell, 0)
      expect(cell.obstacleHp).toBe(OBSTACLE_INFO.stone.hits)
    })
  })

  describe('isObstacleDestroyed', () => {
    it('hp 为 0 时应返回 true', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'ice'
      cell.obstacleHp = 0
      expect(isObstacleDestroyed(cell)).toBe(true)
    })

    it('hp 大于 0 时应返回 false', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'stone'
      cell.obstacleHp = 1
      expect(isObstacleDestroyed(cell)).toBe(false)
    })

    it('hp 等于 0 且无障碍物时应返回 false', () => {
      const cell = createEmptyCell()
      expect(isObstacleDestroyed(cell)).toBe(false)
    })

    it('hp 为负数时应返回 true', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'web'
      cell.obstacleHp = -1
      expect(isObstacleDestroyed(cell)).toBe(true)
    })
  })

  describe('clearObstacle', () => {
    it('应清除 obstacle 标记', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'stone'
      cell.obstacleHp = 2
      clearObstacle(cell)
      expect(cell.obstacle).toBe(null)
    })

    it('应重置 hp 为 0', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'web'
      cell.obstacleHp = 2
      clearObstacle(cell)
      expect(cell.obstacleHp).toBe(0)
    })

    it('应解除 locked 标记', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'stone'
      cell.obstacleHp = 3
      cell.locked = true
      clearObstacle(cell)
      expect(cell.locked).toBe(false)
    })

    it('对无障碍物 cell 应安全调用', () => {
      const cell = createEmptyCell()
      expect(() => clearObstacle(cell)).not.toThrow()
      expect(cell.obstacle).toBe(null)
      expect(cell.obstacleHp).toBe(0)
    })
  })

  describe('HP 系统组合行为', () => {
    it('damageObstacle 达到 0 后 isObstacleDestroyed 应返回 true', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'ice'
      cell.obstacleHp = OBSTACLE_INFO.ice.hits
      for (let i = 0; i < OBSTACLE_INFO.ice.hits; i++) damageObstacle(cell, 1)
      expect(isObstacleDestroyed(cell)).toBe(true)
    })

    it('damageObstacle 未击毁时 isObstacleDestroyed 应返回 false', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'stone'
      cell.obstacleHp = OBSTACLE_INFO.stone.hits
      damageObstacle(cell, 1)
      expect(isObstacleDestroyed(cell)).toBe(false)
    })

    it('clearObstacle 后应使 isObstacleDestroyed 返回 false', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'web'
      cell.obstacleHp = 2
      clearObstacle(cell)
      expect(isObstacleDestroyed(cell)).toBe(false)
    })
  })
})

describe('障碍物系统 - 渲染信息', () => {
  describe('getObstacleRenderInfo', () => {
    it('无障碍物时应返回 null', () => {
      expect(getObstacleRenderInfo(createEmptyCell())).toBeNull()
    })

    it('有宝石无障碍物时应返回 null', () => {
      expect(getObstacleRenderInfo(createGemCell(2))).toBeNull()
    })

    it('冰块应返回完整渲染信息', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'ice'
      cell.obstacleHp = OBSTACLE_INFO.ice.hits
      const info = getObstacleRenderInfo(cell) as ObstacleRenderInfo
      expect(info.type).toBe('ice')
      expect(info.emoji).toBe(OBSTACLE_INFO.ice.emoji)
      expect(info.hp).toBe(OBSTACLE_INFO.ice.hits)
      expect(info.maxHp).toBe(OBSTACLE_INFO.ice.hits)
      expect(info.destroyed).toBe(false)
    })

    it('石头应返回完整渲染信息', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'stone'
      cell.obstacleHp = OBSTACLE_INFO.stone.hits
      const info = getObstacleRenderInfo(cell) as ObstacleRenderInfo
      expect(info.type).toBe('stone')
      expect(info.emoji).toBe(OBSTACLE_INFO.stone.emoji)
      expect(info.hp).toBe(OBSTACLE_INFO.stone.hits)
      expect(info.maxHp).toBe(OBSTACLE_INFO.stone.hits)
      expect(info.destroyed).toBe(false)
    })

    it('蜘蛛网应返回完整渲染信息', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'web'
      cell.obstacleHp = OBSTACLE_INFO.web.hits
      const info = getObstacleRenderInfo(cell) as ObstacleRenderInfo
      expect(info.type).toBe('web')
      expect(info.emoji).toBe(OBSTACLE_INFO.web.emoji)
      expect(info.hp).toBe(OBSTACLE_INFO.web.hits)
      expect(info.maxHp).toBe(OBSTACLE_INFO.web.hits)
      expect(info.destroyed).toBe(false)
    })

    it('hp 为 0 时 destroyed 应为 true', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'web'
      cell.obstacleHp = 0
      expect((getObstacleRenderInfo(cell) as ObstacleRenderInfo).destroyed).toBe(true)
    })

    it('hp 大于 0 时 destroyed 应为 false', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'stone'
      cell.obstacleHp = 1
      expect((getObstacleRenderInfo(cell) as ObstacleRenderInfo).destroyed).toBe(false)
    })

    it('hp 应反映当前值而非 maxHp', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'stone'
      cell.obstacleHp = OBSTACLE_INFO.stone.hits - 1
      const info = getObstacleRenderInfo(cell) as ObstacleRenderInfo
      expect(info.hp).toBe(OBSTACLE_INFO.stone.hits - 1)
      expect(info.maxHp).toBe(OBSTACLE_INFO.stone.hits)
    })

    it('三种障碍物的 emoji 应互不相同', () => {
      const a = createEmptyCell(); a.obstacle = 'ice'; a.obstacleHp = 1
      const b = createEmptyCell(); b.obstacle = 'stone'; b.obstacleHp = 1
      const c = createEmptyCell(); c.obstacle = 'web'; c.obstacleHp = 1
      const ai = (getObstacleRenderInfo(a) as ObstacleRenderInfo).emoji
      const bs = (getObstacleRenderInfo(b) as ObstacleRenderInfo).emoji
      const cw = (getObstacleRenderInfo(c) as ObstacleRenderInfo).emoji
      expect(ai).not.toBe(bs)
      expect(bs).not.toBe(cw)
      expect(ai).not.toBe(cw)
    })

    it('应包含 desc 与 color 字段', () => {
      const cell = createEmptyCell()
      cell.obstacle = 'ice'
      cell.obstacleHp = 1
      const info = getObstacleRenderInfo(cell) as ObstacleRenderInfo
      expect(info.desc).toBeTruthy()
      expect(typeof info.desc).toBe('string')
      expect(info.color).toMatch(/^#[0-9A-Fa-f]{6}$|^rgba\(/)
    })
  })
})

describe('障碍物系统 - placeObstacles', () => {
  it('应正确放置指定数量的冰块', () => {
    const board = createBoard()
    placeObstacles(board, [{ type: 'ice', count: 3 }])
    expect(collectCells(board, c => c.obstacle === 'ice').length).toBe(3)
  })

  it('应正确放置指定数量的石头', () => {
    const board = createBoard()
    placeObstacles(board, [{ type: 'stone', count: 2 }])
    expect(collectCells(board, c => c.obstacle === 'stone').length).toBe(2)
  })

  it('应正确放置指定数量的蜘蛛网', () => {
    const board = createBoard()
    placeObstacles(board, [{ type: 'web', count: 4 }])
    expect(collectCells(board, c => c.obstacle === 'web').length).toBe(4)
  })

  it('应按 OBSTACLE_INFO 初始化 hp', () => {
    const board = createBoard()
    placeObstacles(board, [
      { type: 'ice', count: 1 },
      { type: 'stone', count: 1 },
      { type: 'web', count: 1 },
    ])
    collectCells(board, c => c.obstacle === 'ice').forEach(c =>
      expect(c.obstacleHp).toBe(OBSTACLE_INFO.ice.hits))
    collectCells(board, c => c.obstacle === 'stone').forEach(c =>
      expect(c.obstacleHp).toBe(OBSTACLE_INFO.stone.hits))
    collectCells(board, c => c.obstacle === 'web').forEach(c =>
      expect(c.obstacleHp).toBe(OBSTACLE_INFO.web.hits))
  })

  it('应跳过 null 类型的项', () => {
    const board = createBoard()
    placeObstacles(board, [{ type: null, count: 3 }])
    let total = 0
    for (let r = 0; r < BOARD_SIZE; r++) {
      for (let c = 0; c < BOARD_SIZE; c++) {
        if (board[r][c].obstacle !== null) total++
      }
    }
    expect(total).toBe(0)
  })

  it('应在同一棋盘上处理多种类型', () => {
    const board = createBoard()
    placeObstacles(board, [
      { type: 'ice', count: 2 },
      { type: 'stone', count: 1 },
    ])
    expect(collectCells(board, c => c.obstacle === 'ice').length).toBe(2)
    expect(collectCells(board, c => c.obstacle === 'stone').length).toBe(1)
  })
})

describe('障碍物系统 - Cell 模型', () => {
  it('createEmptyCell 默认无障碍物', () => {
    const cell = createEmptyCell()
    expect(cell.obstacle).toBe(null)
    expect(cell.obstacleHp).toBe(0)
    expect(cell.locked).toBe(false)
  })

  it('放置障碍物的 cell 应保留 obstacle 字段', () => {
    const cell = createEmptyCell()
    cell.obstacle = 'stone'
    cell.obstacleHp = 3
    expect(cell.obstacle).toBe('stone')
    expect(cell.obstacleHp).toBe(3)
  })
})

function collectCells(board: Cell[][], predicate: (cell: Cell) => boolean): Cell[] {
  const cells: Cell[] = []
  for (let r = 0; r < BOARD_SIZE; r++) {
    for (let c = 0; c < BOARD_SIZE; c++) {
      if (predicate(board[r][c])) cells.push(board[r][c])
    }
  }
  return cells
}
