/**
 * LocalEngine - 2048 游戏引擎通信层
 *
 * 调用 WASM 模块（由 emcc 生成的 games.js 提供 window.GameModule）
 * 使用前需先执行: bash engineering/scripts/build-games-wasm.sh
 */

class LocalEngine {
    constructor() {
        this.wasm = null;
        this.ready = false;
    }

    /**
     * 异步加载 WASM 模块
     * games.js 由 emcc 生成，调用 window.GameModule() 获取 WASM 实例
     */
    async init() {
        if (typeof window.GameModule === 'undefined') {
            console.error('[LocalEngine] games.js 未加载，请先执行 bash engineering/scripts/build-games-wasm.sh');
            return;
        }
        this.wasm = await window.GameModule();
        this.ready = true;
        console.log('[LocalEngine] WASM 模块加载完成');
    }

    get wasmReady() {
        return this.ready;
    }

    // ==================== 2048 核心接口 ====================

    /**
     * 创建新游戏
     * @param {number} seed 随机种子
     */
    g2048_create(seed) {
        if (!this.ready) {
            console.warn('[LocalEngine] WASM 未就绪，调用被忽略');
            return;
        }
        this.wasm._g2048_create(seed);
    }

    /**
     * 移动方块
     * @param {number} dir 方向: 0=上, 1=下, 2=左, 3=右
     */
    g2048_move(dir) {
        if (!this.ready) return;
        this.wasm._g2048_move(dir);
    }

    /**
     * 获取指定位置的方块值
     * @param {number} r 行索引 (0-3)
     * @param {number} c 列索引 (0-3)
     * @returns {number} 方块值 (0 表示空格)
     */
    g2048_tile(r, c) {
        if (!this.ready) return 0;
        return this.wasm._g2048_tile(r, c);
    }

    /**
     * 获取当前分数
     * @returns {number} 分数
     */
    g2048_score() {
        if (!this.ready) return 0;
        return this.wasm._g2048_score();
    }

    /**
     * 检查游戏是否结束
     * @returns {boolean} true 表示无法继续移动
     */
    g2048_game_over() {
        if (!this.ready) return false;
        return this.wasm._g2048_game_over();
    }

    /**
     * 检查是否已达成 2048
     * @returns {boolean} true 表示已达成
     */
    g2048_won() {
        if (!this.ready) return false;
        return this.wasm._g2048_won();
    }

    /**
     * 检查是否还能移动
     * @returns {boolean} true 表示至少有一个方向可以移动
     */
    g2048_can_move() {
        if (!this.ready) return false;
        return this.wasm._g2048_can_move();
    }
}
