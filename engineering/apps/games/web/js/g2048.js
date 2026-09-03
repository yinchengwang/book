/**
 * G2048Game - 2048 游戏渲染与输入控制器
 *
 * 使用 Canvas 渲染 4x4 棋盘，支持键盘（方向键/WASD）和重新开始（R）
 * 游戏逻辑由 WASM 引擎（LocalEngine）驱动
 */

/**
 * 2048 游戏控制器
 * @param {string} canvasId Canvas 元素的 ID
 * @param {LocalEngine} engine 游戏引擎实例
 */
class G2048Game {
    constructor(canvasId, engine) {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) {
            throw new Error(`[G2048Game] 未找到 canvas 元素: ${canvasId}`);
        }
        this.ctx = this.canvas.getContext('2d');
        this.engine = engine;

        // Canvas 尺寸：4x4 方格，每格 100px，总计 400x400
        this.cellSize = 100;
        this.padding = 10;
    }

    /**
     * 开始新游戏
     * @param {number} seed 随机种子，默认为当前时间戳
     */
    start(seed = Date.now()) {
        this.engine.g2048_create(seed);
        this.bindInput();
        this.render();
    }

    /**
     * 渲染整个棋盘
     */
    render() {
        const ctx = this.ctx;
        const cs = this.cellSize;
        const pad = this.padding;

        // 清空画布，绘制棋盘背景
        ctx.fillStyle = '#bbada0';
        ctx.fillRect(0, 0, 4 * cs, 4 * cs);

        // 绘制每个格子
        for (let r = 0; r < 4; r++) {
            for (let c = 0; c < 4; c++) {
                const val = this.engine.g2048_tile(r, c);
                const x = c * cs + pad;
                const y = r * cs + pad;
                const size = cs - pad * 2;

                // 格子背景：空格为浅色，有值方块按数字着色
                ctx.fillStyle = val === 0 ? '#cdc1b4' : this.tileColor(val);
                this.roundRect(ctx, x, y, size, size, 6);
                ctx.fill();

                // 绘制数字文字
                if (val !== 0) {
                    ctx.fillStyle = val <= 4 ? '#776e65' : '#f9f6f2';
                    const fontSize = size * 0.45;
                    ctx.font = `bold ${fontSize}px Arial`;
                    ctx.textAlign = 'center';
                    ctx.textBaseline = 'middle';
                    ctx.fillText(val, x + size / 2, y + size / 2);
                }
            }
        }

        // 更新分数显示
        const scoreEl = document.getElementById('score-val');
        if (scoreEl) {
            scoreEl.textContent = this.engine.g2048_score();
        }

        // 检查游戏状态
        const msgEl = document.getElementById('msg');
        if (msgEl) {
            if (this.engine.g2048_game_over()) {
                msgEl.textContent = '游戏结束！按 R 重新开始';
                msgEl.style.color = '#f65e3b';
            } else if (this.engine.g2048_won()) {
                msgEl.textContent = '你赢了！按 R 继续挑战';
                msgEl.style.color = '#edc22e';
            } else {
                msgEl.textContent = '';
            }
        }
    }

    /**
     * 根据方块值返回对应颜色
     * @param {number} val 方块数值
     * @returns {string} CSS 颜色值
     */
    tileColor(val) {
        const colors = {
            2: '#eee4da',
            4: '#ede0c8',
            8: '#f2b179',
            16: '#f59563',
            32: '#f67c5f',
            64: '#f65e3b',
            128: '#edcf72',
            256: '#edcc61',
            512: '#edc850',
            1024: '#edc53f',
            2048: '#edc22e'
        };
        return colors[val] || '#3c3a32';
    }

    /**
     * 绑定键盘输入事件
     */
    bindInput() {
        // 防止重复绑定
        if (this._inputBound) return;
        this._inputBound = true;

        document.addEventListener('keydown', (e) => {
            // 方向映射：0=上, 1=下, 2=左, 3=右, 4=重新开始
            const dirMap = {
                'ArrowUp': 0, 'w': 0, 'W': 0,
                'ArrowDown': 1, 's': 1, 'S': 1,
                'ArrowLeft': 2, 'a': 2, 'A': 2,
                'ArrowRight': 3, 'd': 3, 'D': 3,
                'r': 4, 'R': 4
            };

            const dir = dirMap[e.key];

            if (dir === 4) {
                // 重新开始
                e.preventDefault();
                this.start();
                return;
            }

            if (dir !== undefined) {
                e.preventDefault();
                this.engine.g2048_move(dir);
                this.render();
            }
        });
    }

    /**
     * 绘制圆角矩形（兼容 Canvas）
     * @param {CanvasRenderingContext2D} ctx
     * @param {number} x 左上角 x
     * @param {number} y 左上角 y
     * @param {number} w 宽度
     * @param {number} h 高度
     * @param {number} r 圆角半径
     */
    roundRect(ctx, x, y, w, h, r) {
        ctx.beginPath();
        ctx.moveTo(x + r, y);
        ctx.lineTo(x + w - r, y);
        ctx.quadraticCurveTo(x + w, y, x + w, y + r);
        ctx.lineTo(x + w, y + h - r);
        ctx.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
        ctx.lineTo(x + r, y + h);
        ctx.quadraticCurveTo(x, y + h, x, y + h - r);
        ctx.lineTo(x, y + r);
        ctx.quadraticCurveTo(x, y, x + r, y);
        ctx.closePath();
    }
}
